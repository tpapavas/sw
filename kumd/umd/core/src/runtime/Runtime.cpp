/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <cstdio>
#include <cstring>
#include <sstream>
#include <map>
#include <list>

#include "dlatypes.h"
#include "dlaerror.h"

#include "nvdla_inf.h"
#include "nvdla_os_inf.h"

#include "priv/Emulator.h"
#include "priv/Loadable.h"
#include "priv/Runtime.h"

#include "priv/loadable_generated.h"

#include "ErrorMacros.h"

#include <iomanip>

using std::vector;
using std::stringstream;
using std::string;
using std::endl;
using std::map;
using std::list;

#define STATIC_MEM_BASE_ADDR 0x20000000
#define STATIC_MEM_TOTL_SIZE 0x20000000
#define ALIGN_SIZE 0x40

// Initialize static memory
static char *static_ptr = (char *) STATIC_MEM_BASE_ADDR;;

void *staticAlloc(size_t bytes)
{
    if (static_ptr + bytes < (char *) STATIC_MEM_BASE_ADDR + STATIC_MEM_TOTL_SIZE)
    {
        char *aux_ptr = static_ptr;

        size_t offset = bytes / ALIGN_SIZE * ALIGN_SIZE;
        if (bytes > offset)
            offset += ALIGN_SIZE;

        static_ptr += offset;

        return aux_ptr;
    }

    return NULL;
}

void staticFree(void *ptr)
{
    return;
}

namespace nvdla
{

IRuntime::IRuntime() { }
IRuntime::~IRuntime() { }

IRuntime *createRuntime()
{
    priv::RuntimeFactory::RuntimePrivPair p = priv::RuntimeFactory::newRuntime();
    return p.i();
}

void destroyRuntime(IRuntime *runtime)
{
    priv::RuntimeFactory::deleteRuntime(runtime);
}

namespace priv
{

RuntimeFactory::RuntimePrivPair RuntimeFactory::newRuntime()
{
    IRuntime *runtime;
    Runtime *runtime_priv;
    runtime = runtime_priv = new priv::Runtime();
    if (runtime) {
        s_priv.insert(runtime, runtime_priv);
        s_self.insert(runtime, runtime);
    }
    return RuntimePrivPair(runtime, runtime_priv);
}

void RuntimeFactory::deleteRuntime(IRuntime *runtime)
{
    if (runtime) {
        Runtime *runtime_priv = priv(runtime);
        if (runtime_priv) {
            delete runtime_priv;
        }

        s_priv.remove(runtime);
        s_self.remove(runtime);
    }
}

Runtime *RuntimeFactory::priv(IRuntime *runtime)
{
    BiMap<IRuntime *, Runtime *>::left_iterator f = s_priv.find_left(runtime);

    if ( f == s_priv.end_left() ) {
        return NULL;
    }
    return f->second;
}

IRuntime *RuntimeFactory::i(Runtime *runtime)
{
    BiMap<IRuntime *, Runtime *>::right_iterator f = s_priv.find_right(runtime);
    if ( f == s_priv.end_right() ) {
        return NULL;
    }
    return f->second;
}

IRuntime *RuntimeFactory::self(void *s)
{
    BiMap<void *, IRuntime *>::left_iterator f = s_self.find_left(s);
    if ( f == s_self.end_left() ) {
        return NULL;
    }
    return f->second;
}

BiMap<IRuntime *, Runtime*> RuntimeFactory::s_priv;
BiMap<void *, IRuntime*> RuntimeFactory::s_self;


Runtime::Runtime() :
    IRuntime(),
    m_dla_handle(0),
    m_emu_engine(0),
    h_network_desc_mem(0),
    h_op_desc_mem(0),
    h_surf_desc_mem(0),
    h_dependency_list_mem(0)
{
    m_dla_device_handles[0] = 0;
    m_dla_device_handles[1] = 0;
    m_loaded = 0;
}

Runtime::~Runtime()
{
    // Close all device nodes
    NvDlaClose(m_dla_device_handles[0]);
    NvDlaClose(m_dla_device_handles[1]);
}

bool Runtime::initEMU(void)
{
    bool ok = true;

    // Ping EMU device
    if (!m_emu_engine)
    {
        m_emu_engine = new Emulator();
        m_emu_engine->start();

        // Wait for emulator engine to warm up
        // We should have the ability to timeout here
        while (!m_emu_engine->ping())
        {
            NvDlaSleepMS(200);
        }
    }
    else
    {
        if (!m_emu_engine->ping())
        {
            gLogError << "Emu ping failed (timeout)" << endl;
            ok = false;
        }
    }

    return ok;
}

void Runtime::stopEMU(void)
{
    if (m_emu_engine == NULL)
        return;

    m_emu_engine->stop();
    delete m_emu_engine;
    m_emu_engine = NULL;
}

NvU16 Runtime::getFactoryType() const
{
    return 0;
}

void *Runtime::getDLADeviceContext(size_t sel_i)
{
    // v__NvDlaDebugPrintf("\n[Runtime::getDLADeviceContext] >>> ENTER sel_i= %u\n", sel_i);

    bool ok = true;
    NvDlaError err;

    if (sel_i > 0) {
        std::cout << "[Runtime::getDLADeviceContext] sel_i > 0 -> not supported, ok=false\n";
        ok = false;
        goto done;
    }

    std::cout << "[Runtime::getDLADeviceContext] m_dla_device_handles[" << sel_i
              << "] before = " << m_dla_device_handles[sel_i] << "\n";

    if ( !m_dla_device_handles[sel_i] ) {
        std::cout << "[Runtime::getDLADeviceContext] handle is NULL, calling NvDlaInitialize\n";

        err = NvDlaInitialize(&m_dla_handle);
        std::cout << "[Runtime::getDLADeviceContext] NvDlaInitialize -> err=" << err
                  << " m_dla_handle=" << m_dla_handle << "\n";
        ok = err == NvDlaSuccess;
        if ( !ok ) {
            std::cout << "[Runtime::getDLADeviceContext] NvDlaInitialize FAILED\n";
            goto done;
        }

        std::cout << "[Runtime::getDLADeviceContext] calling NvDlaOpen with sel_i=" << sel_i << "\n";
        err = NvDlaOpen((void *)m_dla_handle, sel_i, (void **)&m_dla_device_handles[sel_i]);
        std::cout << "[Runtime::getDLADeviceContext] NvDlaOpen -> err=" << err
                  << " device_handle=" << m_dla_device_handles[sel_i] << "\n";
        ok = err == NvDlaSuccess;
        if ( !ok ) {
            gLogError << "failed to open dla device" << endl;
            std::cout << "[Runtime::getDLADeviceContext] NvDlaOpen FAILED, zeroing handle\n";
            m_dla_device_handles[sel_i] = 0;
        }
    } else {
        std::cout << "[Runtime::getDLADeviceContext] handle already exists: "
                  << m_dla_device_handles[sel_i] << "\n";
    }

 done:
    if ( ok ) {
        std::cout << "[Runtime::getDLADeviceContext] <<< EXIT OK, returning handle="
                  << m_dla_device_handles[sel_i] << "\n";
        return m_dla_device_handles[sel_i];
    }
    std::cout << "[Runtime::getDLADeviceContext] <<< EXIT ERROR, returning 0\n";
    return 0;
}


NvU16 Runtime::getMaxDevices()
{
    return getMaxDLADevices();
}

NvU16 Runtime::getNumDevices()
{
    NvU16 num_devs = 0;

    for ( size_t di = 0; di < getMaxDLADevices(); ++di ) {
        if ( getDLADeviceContext(di) ) {
            num_devs++;
        }
    }

    return num_devs;
}

bool Runtime::versionsCompatible(const ILoadable::Version &a, const ILoadable::Version &b)
{
    return (a.major == b.major) && (a.minor == b.minor);
}

bool Runtime::load(NvU8 *buf, int instance)
{
    NvDlaError e = NvDlaSuccess;
    ILoadable *i_loadable;
    Loadable *loadable;

    bool ok = true;

    i_loadable = LoadableFactory::deserializeLoadable(buf);
    if ( !i_loadable )
    {
        ok = false;
        goto done;
    }

    loadable = LoadableFactory::priv(i_loadable);

    if ( instance >= 0 )
    {
        if ( instance >= getNumDevices() )
        {
            gLogError << "Out of bounds DLA instance " << instance << " requested." << endl;
            ok = false;
            goto done;
        }

        m_loaded_instance = size_t(instance);
        std::cout << " [load] instance=" << instance << std::endl;
    }
    else
    {
        m_loaded_instance = 0;
    }

    // std::cout<< "[load] Pretty dump of loadable at runtime\n";
    // loadable->dump(std::cout, /*hexdump=*/false);        
    m_task_entries   = loadable->getTaskListEntries();
    m_submit_entries = loadable->getSubmitListEntries();
    m_memory_entries = loadable->getMemoryListEntries();
    m_address_entries = loadable->getAddressListEntries();
    m_tensor_desc_entries = loadable->getTensorDescListEntries();
    m_reloc_entries = loadable->getRelocEntries();

    std::cout << "[Runtime] m_task_entries.size()   = " << m_task_entries.size() << "\n";
    std::cout << "[Runtime] m_task_entries.data()   = " << (const void*)m_task_entries.data() << "\n";

    std::cout << "[Runtime] m_submit_entries.size()   = " << m_submit_entries.size() << "\n";
    std::cout << "[Runtime] m_submit_entries.data()   = " << (const void*)m_submit_entries.data() << "\n";

    std::cout << "[Runtime] m_memory_entries.size()   = " << m_memory_entries.size() << "\n";
    std::cout << "[Runtime] m_memory_entries.data()   = " << (const void*)m_memory_entries.data() << "\n";

    std::cout << "[Runtime] m_address_entries.size()   = " << m_address_entries.size() << "\n";
    std::cout << "[Runtime] m_address_entries.data()   = " << (const void*)m_address_entries.data() << "\n";

    std::cout << "[Runtime] m_tensor_desc_entries.size()   = " << m_submit_entries.size() << "\n";
    std::cout << "[Runtime] m_tensor_desc_entries.data()   = " << (const void*)m_submit_entries.data() << "\n";

    std::cout << "[Runtime] m_reloc_entries.size()   = " << m_reloc_entries.size() << "\n";
    std::cout << "[Runtime] m_reloc_entries.data()   = " << (const void*)m_reloc_entries.data() << "\n";



    if ( debugStrideRewrite() )
    {
        gLogInfo << "runtime sees loadable gave back " << m_reloc_entries.size() << " reloc entries" << endl;
        // tbd: stash the per-address id reloc entries for quicker scanning
    }

    if ( m_submit_entries.size() < 1 || m_task_entries.size() < 1 || m_memory_entries.size() < 1 ) {
        gLogError << "need at least one submit task and memory entry to load" << endl;
        ok = false;
        goto done;
    }

    m_memory.resize(m_memory_entries.size());
    if ( debugMemoryLayout() )
    {
        gLogInfo << "load memory list entries=" << m_memory.size() << endl;
    }

    for ( size_t mi = 0, MI = m_memory_entries.size(); mi != MI; ++mi ) {
        m_memory[mi] = Memory(m_memory_entries[mi]);
        std::cout <<"load\t id=" << m_memory[mi].id() <<
            " size=" << m_memory[mi].size() <<
            " alignment=" << m_memory[mi].alignment() <<
            " domain=" << (int)m_memory[mi].domain() <<
            " virt addr= " << m_memory[mi].getVirtAddr() <<
            " memory handler=" << m_memory[mi].getHandle() <<
            " flags=" << (int)m_memory[mi].flags() << endl;
    }

    std::cout << "[Runtime] m_memory.size()   = " << m_memory.size() << "\n";
    std::cout << "[Runtime] m_memory.data()   = " << (const void*)m_memory.data() << "\n";


    PROPAGATE_ERROR( initBindableMemory() );

    //
    // for all entries hit their load methods.
    // some might not require work yet (io entries, etc).
    // but some may trigger allocation and filling of
    // items/ events.

    for ( size_t mi = 0, MI = m_memory_entries.size(); mi != MI; ++mi ) {
        PROPAGATE_ERROR_FAIL( loadMemory(loadable, &m_memory[mi]) );
    }

    {   
        dumpAllTaskBlobs();
        dumpAllTensorBlobs(1000000000);

        std::cout << "[Runtime] After loadMemory loop\n";
        std::cout << "[Runtime] Lets see the inside of the memorys\n";

        int size_of_vector_mems = m_memory.size();
        for (int idx_mem = 0; idx_mem < size_of_vector_mems; idx_mem++) {
            void *pointer_on_buffer = m_memory[idx_mem].getVirtAddr();
            
            std::cout << "  Memory idx=" << idx_mem
                    << " handle=" << m_memory[idx_mem].getHandle()
                    << " virt_addr=" << pointer_on_buffer
                    << " size=" << m_memory[idx_mem].size() << "\n";

            //unsigned char *p = (unsigned char*)pointer_on_buffer;

           // if(m_memory[idx_mem].flags() & ILoadable::MemoryListEntry::flags_set()){
           //     /*
           //     for (int idx = 0; idx <  m_memory[idx_mem].size(); idx++) {
           //         //p[idx] = 0xAA;
           //         printf("    p[%d] = 0x%02x\n", idx, p[idx]);
           //     }
           //     */
           // }

        }
    }
    std::cout << "[Runtime] Finished loadMemory loop\n";

    m_address.resize(m_address_entries.size());
    if ( debugMemoryLayout() )
    {
        gLogInfo << "load address list entries=" << m_address.size() << endl;
    }

    for ( size_t ai = 0, AI = m_address_entries.size(); ai != AI; ++ai ) {
        m_address[ai] = Address(m_address_entries[ai]);
        if ( debugMemoryLayout() )
        {
            gLogInfo << "load\t id=" << m_address[ai].id() <<
                " mem_id=" << m_address[ai].mem_id() <<
                " offset=" << m_address[ai].offset() << endl;
        }
    }

    m_task.resize(m_task_entries.size());
    if ( debugTasks() )
    {
        gLogInfo << "load num tasks=" << m_task.size() << endl;
    }

    m_numDLATasks = 0;

    for ( size_t ti = 0, TI = m_task_entries.size(); ti != TI; ++ti )
    {
        m_task[ti] = Task(m_task_entries[ti]);

        //
        // check task entries for explicit dla instance assignments
        // and complain about non-zero requests.  the production
        // runtime doesn't/won't support explicit assignments of
        // dla instances in the loadable.
        //

        if ( m_task[ti].interface() == ILoadable::Interface_DLA1 )
        {
            if ( m_task[ti].instance() != ILoadable::TaskListEntry::instance_ANY() )
            {
                gLogWarning << "the loadable specified dla instance " <<
                    m_task[ti].instance() << " which will be ignored.";
            }
            m_numDLATasks++;
        }

        if ( debugTasks() )
        {
            gLogInfo << "load\ttask id=" << ti << " address list entries=" <<
                m_task[ti].address_list().size() << endl;
        }
    }

    m_submit.resize(m_submit_entries.size());

    for ( size_t si = 0, SI = m_submit_entries.size(); si != SI; ++si ) {
        m_submit[si] = Submit(m_submit_entries[si]);
        if ( debugTasks() )
        {
            gLogInfo << "load\tsubmit id" << si << " tasks=" << m_submit[si].tasks().size() << endl;
        }
    }

    ok = true;
    m_loaded = loadable;

 done:
    return ok;

 fail:
    return false;
}

void Runtime::unload()
{
    // Free all non binded memories
    for ( size_t mi = 0, MI = m_memory_entries.size(); mi != MI; ++mi ) {
        unloadMemory(&m_memory[mi]);
    }

    m_task_entries.clear();
    m_submit_entries.clear();
    m_memory_entries.clear();
    m_address_entries.clear();
    m_tensor_desc_entries.clear();
    m_reloc_entries.clear();

    m_task.clear();
    m_submit.clear();
    m_memory.clear();
    m_event.clear();
    m_address.clear();
    m_tensor_desc.clear();

    if (m_loaded)
        LoadableFactory::deleteLoadable(LoadableFactory::i(m_loaded));
    m_loaded = 0;
}

//
// probably need a bit more after rebind...
//
bool Runtime::bindInputTensor(int index, void *hMem)
{
    std::vector<Memory *>bind_to_mem;
    bool ok = true;
    if ( index < 0 ) {
        ok = false;
        goto done;
    }

    // determine which mem needs to be rebound
    for ( size_t mi = 0, MI = m_memory.size(); mi != MI; ++mi ) {
        if ( m_memory[mi].inputBindId() == index ) {
            bind_to_mem.push_back( &m_memory[mi] );
        }
    }

    // unlikely to be > size 1, but...
    for (size_t bmi = 0, BMI = bind_to_mem.size(); bmi != BMI; ++bmi ) {
        bind_to_mem[bmi]->setHandle(hMem);
        bind_to_mem[bmi]->setVirtAddr(m_hmem_memory_map[hMem]);
    }

 done:
    return ok;
}

bool Runtime::bindOutputTensor(int index, void *hMem)
{
    bool ok = true;
    std::vector<Memory *> bind_to_mem;
    if ( index < 0 ) {
        ok = false;
        goto done;
    }

    // determine which mem needs to be rebound
    for ( size_t mi = 0, MI = m_memory.size(); mi != MI; ++mi ) {
        if ( m_memory[mi].outputBindId() == index ) {
            bind_to_mem.push_back( &m_memory[mi] );
        }
    }

    // unlikely to be > size 1, but...
    for (size_t bmi = 0, BMI = bind_to_mem.size(); bmi != BMI; ++bmi ) {
        bind_to_mem[bmi]->setHandle(hMem);
        bind_to_mem[bmi]->setVirtAddr(m_hmem_memory_map[hMem]);
    }

done:
    return ok;
}

bool Runtime::fillTaskAddressList(Task *task, NvDlaTask *dla_task)
{
    std::cout << "\n[Runtime::fillTaskAddressList] >>> ENTER for task_id=" << task->id() << "\n";

    size_t num_memory_ids = m_memory.size();
    size_t num_task_addr_list_entries = task->mEntry.address_list.size();

    std::cout << "[Runtime::fillTaskAddressList] m_memory.size()=" << num_memory_ids
              << " num_task_addr_list_entries=" << num_task_addr_list_entries << "\n";

    if ( num_task_addr_list_entries > NVDLA_MAX_BUFFERS_PER_TASK )
    {
        std::cout << "[Runtime::fillTaskAddressList] ERROR: num_task_addr_list_entries ("
                  << num_task_addr_list_entries << ") > NVDLA_MAX_BUFFERS_PER_TASK("
                  << NVDLA_MAX_BUFFERS_PER_TASK << ")\n";
        gLogError << "too many address list entries." << endl;
        return false;
    }

    dla_task->num_addresses = num_task_addr_list_entries;
    std::cout << "[Runtime::fillTaskAddressList] dla_task->num_addresses="
              << dla_task->num_addresses << "\n";

    for ( size_t ali = 0, ALI = num_task_addr_list_entries; ali != ALI; ++ali )
    {

        NvS16 address_list_entry_id = task->mEntry.address_list[ali];
        std::cout << "[Runtime::fillTaskAddressList]  ali=" << ali
                  << " address_list_entry_id=" << address_list_entry_id << "\n";

        if ( ! ( (address_list_entry_id >= 0) && (size_t(address_list_entry_id) < m_address.size() )) )
        {
            std::cout << "[Runtime::fillTaskAddressList]  ERROR: address_list_entry_id out of range "
                      << "(m_address.size()=" << m_address.size() << ")\n";
            gLogError << "dla address list entry=" << ali << " id=" << address_list_entry_id << " is bogus" << endl;
            return false;
        }

        NvS16 memory_id = m_address[address_list_entry_id].mem_id();
        std::cout << "[Runtime::fillTaskAddressList]   m_address[address_list_entry_id].id=" << m_address[address_list_entry_id].id() << "\n";
        std::cout << "[Runtime::fillTaskAddressList]   mapped to memory_id=" << memory_id << "\n";
        size_t num_memory_ids_local = num_memory_ids;
        // there should be valid handles at every ali/mem_id for a dla task.
        if ( ! ((memory_id >= 0) && (size_t(memory_id) < num_memory_ids) ))
        {
            std::cout << "[Runtime::fillTaskAddressList]  ERROR: mem_id out of bounds: "
                      << memory_id << " (num_memory_ids=" << num_memory_ids_local << ")\n";
            gLogError << "mem_id=" << memory_id << " out of bounds." << endl;
            return false;
        }

        if (m_memory[memory_id].hMem == 0)
        {
            std::cout << "[Runtime::fillTaskAddressList]  ERROR: m_memory[" << memory_id
                      << "].hMem == 0 (NULL handle)\n";
            gLogError << __func__ << " ali=" << ali << " -> mem_id=" << memory_id << " has a null memory handle." << endl;
            return false;
        }

        Memory *mem = &m_memory[memory_id];
        void *hMem   = mem->getHandle();
        NvU64 offset = m_address[address_list_entry_id].mEntry.offset;

        std::cout << "[Runtime::fillTaskAddressList]   Memory mapping:"
                  << " ali=" << ali
                  << " mem=" << mem
                  << " hMem=" << hMem
                  << " offset=0x" << std::hex << offset << std::dec
                  << " vAddr=" << mem->getVirtAddr()
                  << "\n";

        dla_task->address_list[ali].handle = hMem;
        dla_task->address_list[ali].offset = m_address[address_list_entry_id].mEntry.offset;
        dla_task->address_list[ali].vAddr = mem->getVirtAddr();
    }

    std::cout << "[Runtime::fillTaskAddressList] <<< EXIT OK\n";
    return true;
}


bool Runtime::fillEMUTaskAddressList(Task *task, EMUTaskDescAccessor taskDescAcc)
{
    std::cout << "\n[Runtime::fillEMUTaskAddressList] >>> ENTER for task_id=" << task->id() << "\n";

    size_t num_memory_ids = m_memory.size();
    size_t num_task_addr_list_entries = task->mEntry.address_list.size();

    std::cout << "[Runtime::fillEMUTaskAddressList] m_memory.size()=" << num_memory_ids
              << " num_task_addr_list_entries=" << num_task_addr_list_entries
              << " maxBuffersPerTask()=" << taskDescAcc.maxBuffersPerTask() << "\n";

    if ( debugTasks() || debugMemoryLayout() )
    {
        gLogInfo << "filling emu task_id=" << task->id() << " address list entries=" << num_task_addr_list_entries << endl;
    }

    if ( num_task_addr_list_entries > taskDescAcc.maxBuffersPerTask() )
    {
        std::cout << "[Runtime::fillEMUTaskAddressList] ERROR: num_task_addr_list_entries ("
                  << num_task_addr_list_entries << ") > maxBuffersPerTask("
                  << taskDescAcc.maxBuffersPerTask() << ")\n";
        gLogError << __func__ << " too many address list entries." << endl;
        return false;
    }

    *taskDescAcc.numAddresses() = num_task_addr_list_entries;
    std::cout << "[Runtime::fillEMUTaskAddressList] *numAddresses()="
              << *taskDescAcc.numAddresses() << "\n";

    for ( size_t ali = 0, ALI = num_task_addr_list_entries; ali != ALI; ++ali )
    {

        NvS16 address_list_entry_id = task->mEntry.address_list[ali];
        std::cout << "[Runtime::fillEMUTaskAddressList]  ali=" << ali
                  << " address_list_entry_id=" << address_list_entry_id << "\n";

        if ( ! ( (address_list_entry_id >= 0) && (size_t(address_list_entry_id) < m_address.size() )) )
        {
            std::cout << "[Runtime::fillEMUTaskAddressList]  ERROR: address_list_entry_id out of range "
                      << "(m_address.size()=" << m_address.size() << ")\n";
            gLogError << __func__ << " address list entry=" << ali << " id=" << address_list_entry_id << " is bogus" << endl;
            return false;
        }

        NvS16 memory_id = m_address[address_list_entry_id].mem_id();
        std::cout << "[Runtime::fillEMUTaskAddressList]   mapped to memory_id=" << memory_id << "\n";

        if ( ! ((memory_id >= 0) && (size_t(memory_id) < num_memory_ids) ))
        {
            std::cout << "[Runtime::fillEMUTaskAddressList]  ERROR: memory id out of bounds: "
                      << memory_id << " (num_memory_ids=" << num_memory_ids << ")\n";
            gLogError << __func__ << " memory id out of bounds: " << memory_id << endl;
            return false;
        }

        Memory *mem = &m_memory[memory_id];
        void *hMem = mem->getVirtAddr();
        NvU64         offset = m_address[address_list_entry_id].mEntry.offset;

        std::cout << "[Runtime::fillEMUTaskAddressList]   Memory mapping:"
                  << " ali=" << ali
                  << " mem=" << mem
                  << " virt=" << hMem
                  << " domain=" << mem->domain()
                  << " offset=0x" << std::hex << offset << std::dec
                  << "\n";

        if ( mem->domain() == ILoadable::MemoryListEntry::domain_sram() )
        {
            std::cout << "[Runtime::fillEMUTaskAddressList]   domain_sram -> hMem=0, offset=0\n";
            hMem = 0;
            offset  = 0;
        }

        *((void **)taskDescAcc.addressList(ali).hMem()) = hMem;
        *taskDescAcc.addressList(ali).offset() = offset;

        if ( debugTasks() || debugMemoryLayout() )
        {
            gLogInfo << "\tali=" << ali << " offset=" << offset << " handle=" << hMem << endl;
        }


    }

    std::cout << "[Runtime::fillEMUTaskAddressList] <<< EXIT OK\n";
    return true;
}

bool Runtime::submit()
{
    NvDlaError e = NvDlaSuccess;
    e = submitInternal();
    return e == NvDlaSuccess;
}

NvDlaError Runtime::submitInternal()
{
    NvDlaError e = NvDlaSuccess;
    Task *task;

    vector<EMUTaskDescAccessor*> emu_task_descs;
    size_t ii;
    size_t num_emu_instances;

    bool ok = true;
    NVDLA_UNUSED(ok);

    std::cout << "\n[Runtime::submitInternal] >>> ENTER\n";
    std::cout << "[Runtime::submitInternal] m_loaded=" << m_loaded
              << " m_task.size()=" << m_task.size()
              << " m_submit.size()=" << m_submit.size() << "\n";

    if ( !m_loaded ) {
        std::cout << "[Runtime::submitInternal] ERROR: m_loaded == false\n";
        ORIGINATE_ERROR_FAIL(NvDlaError_InvalidState, "exec requires a successful load first");
    }

    if ( !m_task.size() ) {
        std::cout << "[Runtime::submitInternal] ERROR: m_task.size() == 0\n";
        ORIGINATE_ERROR_FAIL(NvDlaError_InvalidState, "no tasks to exec");
    }

    if ( !m_submit.size() ) {
        std::cout << "[Runtime::submitInternal] ERROR: m_submit.size() == 0\n";
        ORIGINATE_ERROR_FAIL(NvDlaError_InvalidState, "no submission sets to exec");
    }

    std::cout << "[Runtime::submitInternal] Reloading dep_graph memories if needed...\n";

    // Force reload dependency graph contents from the loadable to
    // satisfy firmware requirements
    for ( size_t mi = 0, MI = m_memory_entries.size(); mi != MI; ++mi )
    {
        Memory* memory = &m_memory[mi];

        std::cout << "[Runtime::submitInternal] Checking memory id=" << memory->id() 
                  << " hmem=" << memory->getHandle()
                  << " virt=" << memory->getVirtAddr()
                  << " flags=" << (int) memory->flags()
                  << "\n";
        if ( memory->flags() & ILoadable::MemoryListEntry::flags_set() )
        {
            std::cout << "[Runtime::submitInternal]  memory id=" << memory->id()
                      << " has flags_set, contents.size()="
                      << memory->contents().size() << "\n";

            for ( vector<string>::iterator ci = memory->contents().begin(); ci != memory->contents().end(); ++ci )
            {
                std::cout << "[Runtime::submitInternal]   content symbol=\""
                          << *ci << "\"\n";

                if ((*ci).find("dep_graph") != std::string::npos)
                {
                    std::cout << "[Runtime::submitInternal]   -> matches \"dep_graph\", calling loadMemory()\n";
                    PROPAGATE_ERROR_FAIL( loadMemory(m_loaded, &m_memory[mi]) );
                    std::cout << "[Runtime::submitInternal]   loadMemory() OK for memory id=" << memory->id() << "\n";
                }
            }
        }
    }

    num_emu_instances = 1;
    std::cout << "[Runtime::submitInternal] num_emu_instances=" << num_emu_instances << "\n";
    std::cout << "[Runtime::submitInternal] m_submit.size()=" << m_submit.size() << "\n";

    for ( size_t ss=0; ss < m_submit.size(); ss++ ) {

        std::cout << "\n[Runtime::submitInternal] --- Submission set ss=" << ss
                  << " tasks.size()=" << m_submit[ss].tasks().size() << " ---\n";     
        size_t emu_instance = 0;

        for ( ii=0; ii < m_submit[ss].tasks().size(); ii++ )
        {
            size_t task_id = m_submit[ss].tasks()[ii];

            std::cout << "[Runtime::submitInternal]  Handling task index ii=" << ii
                      << " (task_id=" << task_id << ")\n";

            if ( task_id >= m_task.size() ) {
                std::cout << "[Runtime::submitInternal]  ERROR: task_id >= m_task.size() ("
                          << task_id << " >= " << m_task.size() << ")\n";
                ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "task id out of range");
            }

            task = &m_task[ task_id ];

            std::cout << "[Runtime::submitInternal]  Task:"
                      << " id=" << task->id()
                      << " interface=" << (int)task->interface()
                      << " instance=" << task->instance()
                      << "\n";
            switch ( task->interface() ) {

                case ILoadable::Interface_DLA1:
                {
                    std::cout << "[Runtime::submitInternal]   -> Interface_DLA1\n";

                    void *dev;
                    NvDlaTask dla_task;

                    std::string task_blob_name = "task-"+std::to_string(task_id);
                    std::cout << "[KUMD]: task_blob_name: " << task_blob_name << "\n";
                    Runtime::TaskBlobs *task_blob = &m_taskBlobs[task_blob_name];
                    auto net = reinterpret_cast<dla_network_desc*>(task_blob->addr0);
                    struct dla_common_op_desc *deps = reinterpret_cast<struct dla_common_op_desc*>(task_blob->dep_graph);
                    union dla_operation_container *ops = reinterpret_cast<union dla_operation_container*>(task_blob->op_list);
                    union dla_surface_container *surfs = reinterpret_cast<union dla_surface_container*>(task_blob->surf_list);
                    struct dla_lut_param *luts = reinterpret_cast<struct dla_lut_param*>(task_blob->lut_list);

                    dev = getDLADeviceContext(m_loaded_instance);
                    std::cout << "[Runtime::submitInternal] Done getDLADeviceContext(m_loaded_instance="
                              << m_loaded_instance << ") -> " << dev << "\n";

                    std::memset(&dla_task, 0, sizeof(dla_task));

                    dla_task.task_id = task->id();
                    std::cout << "[Runtime::submitInternal]   dla_task.task_id=" << dla_task.task_id << "\n";

                    std::cout << "[Runtime::submitInternal]   Calling fillTaskAddressList()\n";
                    fillTaskAddressList(task, &dla_task);
                    std::cout << "[Runtime::submitInternal]   fillTaskAddressList() done\n";

                    std::cout << "[Runtime::submitInternal]   Calling NvDlaSubmit(DLA1)\n";
                    PROPAGATE_ERROR_FAIL( NvDlaSubmit(NULL, dev, &dla_task, 1, net, deps, ops, surfs, luts, num_dlas, num_batches) );
                    std::cout << "[Runtime::submitInternal]   NvDlaSubmit(DLA1) OK\n";
                }
                break;

                case ILoadable::Interface_EMU1:
                {
                    std::cout << "[Runtime::submitInternal]   -> Interface_EMU1\n";

                    EMUInterface *emu_if = new EMUInterfaceA();
                    std::cout << "[Runtime::submitInternal]   emu_if @ " << emu_if << "\n";

                    NvU8* task_mem = new NvU8[emu_if->taskDescAccessor(0).struct_size()];
                    std::cout << "[Runtime::submitInternal]   task_mem @ " << static_cast<void*>(task_mem)
                              << " size=" << emu_if->taskDescAccessor(0).struct_size() << "\n";

                    std::memset(task_mem, 0, emu_if->taskDescAccessor(0).struct_size());

                    EMUTaskDescAccessor emu_task_desc = emu_if->taskDescAccessor(task_mem);
                    std::cout << "[Runtime::submitInternal]   EMUTaskDescAccessor created\n";
                    emu_task_descs.push_back(&emu_task_desc);
                    std::cout << "[Runtime::submitInternal]   emu_task_descs.size()="
                              << emu_task_descs.size() << "\n";

                    if (!m_emu_engine)
                    {
                        std::cout << "[Runtime::submitInternal]   ERROR: m_emu_engine == NULL\n";
                        ORIGINATE_ERROR_FAIL(NvDlaError_NotInitialized);
                    }

                    if ( task->instance() != ILoadable::TaskListEntry::instance_ANY() ) {
                        std::cout << "[Runtime::submitInternal]   task->instance()="
                                  << task->instance() << ", num_emu_instances=" << num_emu_instances << "\n";
                        if ( task->instance() < (int)num_emu_instances ) {
                            emu_instance = task->instance();
                        } else {
                            std::cout << "[Runtime::submitInternal]   ERROR: emu instance out of bounds\n";
                            ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "emu instance out of bounds");
                        }
                    }

                    std::cout << "[Runtime::submitInternal]   Calling fillEMUTaskAddressList()\n";
                    fillEMUTaskAddressList(task, *(emu_task_descs.back()));
                    std::cout << "[Runtime::submitInternal]   fillEMUTaskAddressList() done\n";

                    std::cout << "[Runtime::submitInternal]   Calling m_emu_engine->submit()\n";
                    PROPAGATE_ERROR_FAIL( m_emu_engine->submit(task_mem, 1) );
                    std::cout << "[Runtime::submitInternal]   m_emu_engine->submit() OK\n";

                    emu_instance = (emu_instance + 1) % num_emu_instances;
                    std::cout << "[Runtime::submitInternal]   emu_instance now " << emu_instance << "\n";

                    /* since blocking call, deleting here should be fine.*/
                    delete[] task_mem;
                    std::cout << "[Runtime::submitInternal]   deleted task_mem\n";
                    delete emu_if;
                    std::cout << "[Runtime::submitInternal]   deleted emu_if\n";
                }
                break;

                default:
                    std::cout << "[Runtime::submitInternal]   ERROR: unrecognized interface "
                              << (int)task->interface() << "\n";
                    ok = false;
                    ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "unrecognized interface %d", task->interface());
                    break;

            } // switch on engine type

        } // each task in a submission

        std::cout << "[Runtime::submitInternal] --- End of submission set ss=" << ss << " ---\n";
    } // each submission set

fail:
    std::cout << "[Runtime::submitInternal] <<< EXIT e=" << e << "\n";
    return e;
}


NvDlaError Runtime::allocateSystemMemory(void **phMem, NvU64 size, void **pData)
{
    std::cout << "\n[Runtime::allocateSystemMemory] >>> ENTER\n";
    std::cout << "[Runtime::allocateSystemMemory]  phMem addr=" << phMem
              << " *phMem(before)=" << (phMem ? *phMem : nullptr) << "\n";
    std::cout << "[Runtime::allocateSystemMemory]  pData addr=" << pData
              << " *pData(before)=" << (pData ? *pData : nullptr) << "\n";
    std::cout << "[Runtime::allocateSystemMemory]  size=" << (unsigned long long)size << "\n";

    NvDlaError e = NvDlaSuccess;
    void *hDla = getDLADeviceContext(m_loaded_instance);
    std::cout << "[Runtime::allocateSystemMemory]  hDla=" << hDla << "\n";

    std::cout << "[Runtime::allocateSystemMemory]  Calling NvDlaAllocMem(NULL, hDla, phMem, pData, size, NvDlaHeap_System)\n";

    /* Allocate memory for network */
    PROPAGATE_ERROR_FAIL( NvDlaAllocMem(NULL, hDla, phMem, pData, size, NvDlaHeap_System) );
    *pData = staticAlloc(size);

    std::cout << "[Runtime::allocateSystemMemory]  NvDlaAllocMem SUCCESS\n";
    std::cout << "[Runtime::allocateSystemMemory]  *phMem(after)=" << (phMem ? *phMem : nullptr)
              << " *pData(after)=" << (pData ? *pData : nullptr) << "\n";

    m_hmem_memory_map.insert(std::make_pair(*phMem, *pData));
    NvDlaDebugPrintf("[NvDlaError Runtime::allocateSystemMemory]  m_hmem_memory_map.insert(std::make_pair(*phMem, *pData));\n");
    std::cout << "[Runtime::allocateSystemMemory]  Inserted into m_hmem_memory_map: key(*phMem)="
              << *phMem << " value(*pData)=" << *pData << "\n";
    std::cout << "[Runtime::allocateSystemMemory]  m_hmem_memory_map.size() now="
              << m_hmem_memory_map.size() << "\n";
    std::cout << "[Runtime::allocateSystemMemory] <<< EXIT success\n";

    return NvDlaSuccess;

fail:
    std::cout << "[Runtime::allocateSystemMemory]  NvDlaAllocMem FAILED, cleaning up\n";
    std::cout << "[Runtime::allocateSystemMemory]  *phMem(before reset)="
              << (phMem ? *phMem : nullptr)
              << " *pData(before reset)="
              << (pData ? *pData : nullptr) << "\n";

    *phMem = NULL;
    *pData = NULL;

    std::cout << "[Runtime::allocateSystemMemory]  *phMem(after reset)=" << *phMem
              << " *pData(after reset)=" << *pData << "\n";
    std::cout << "[Runtime::allocateSystemMemory] <<< EXIT error e=" << e << "\n";

    return e;
}

void Runtime::freeSystemMemory(void *phMem, NvU64 size)
{
    void *hDla = getDLADeviceContext(m_loaded_instance);
    void *pData = m_hmem_memory_map[phMem];

    /* Free memory */
    // NvDlaFreeMem(NULL, hDla, phMem, pData, size);
    m_hmem_memory_map.erase(phMem);
}

void Runtime::unloadMemory(Memory *memory)
{
    if (! (memory->flags() & ILoadable::MemoryListEntry::flags_alloc()))
        return;

    if (memory->bindable())
        return;

    if (memory->domain() == ILoadable::MemoryListEntry::domain_sysmem()) {
        void *hDla = getDLADeviceContext(m_loaded_instance);
        void *hMem = memory->getHandle();
        void *pData = memory->getVirtAddr();
        NvU64 size = memory->size();

        // TODO: unmap the memory before freeing
        NvDlaFreeMem(NULL, hDla, hMem, pData, size);
        memory->setHandle(0);
        memory->setVirtAddr(0);
    }
}

const char* Runtime::opTypeToStr(uint8_t op_type)
{
    switch (op_type) {
        case DLA_OP_BDMA:  return "BDMA";
        case DLA_OP_CONV:  return "CONV";
        case DLA_OP_SDP:   return "SDP";
        case DLA_OP_PDP:   return "PDP";
        case DLA_OP_CDP:   return "CDP";
        case DLA_OP_RUBIK: return "RUBIK";
        default:           return "UNKNOWN";
    }
}

void Runtime::dumpAllTaskBlobs()
{
    std::cout << "\n================ DLA TASK BLOBS DUMP ================\n";
    if (m_taskBlobs.empty()) {
        std::cout << "[dumpAllTaskBlobs] no tasks recorded yet\n";
        return;
    }

    for (auto &kv : m_taskBlobs) {
        const std::string &name = kv.first;  // "task-0", "task-1", ...
        TaskBlobs &tb           = kv.second;

        std::cout << "\n-----------------------------------------------------\n";
        std::cout << "Task '" << name << "'\n";
        std::cout << "  addr0      = " << static_cast<void*>(tb.addr0)
                  << " (" << tb.addr0_size << " bytes)\n";
        std::cout << "  dep_graph  = " << static_cast<void*>(tb.dep_graph)
                  << " (" << tb.dep_size << " bytes)\n";
        std::cout << "  op_list    = " << static_cast<void*>(tb.op_list)
                  << " (" << tb.op_size << " bytes)\n";
        std::cout << "  surf_list  = " << static_cast<void*>(tb.surf_list)
                  << " (" << tb.surf_size << " bytes)\n";
        std::cout << "  lut_list   = " << static_cast<void*>(tb.lut_list)
                  << " (" << tb.lut_size << " bytes)\n";

        dumpOneTask(name, tb);
    }

    std::cout << "\n================ END DLA TASK BLOBS DUMP ============\n";
}

void Runtime::dumpOneTask(const std::string& name, TaskBlobs& tb)
{
    if (!tb.addr0) {
        std::cout << "[dumpOneTask] " << name << ": no addr0, skipping\n";
        return;
    }

    auto net = reinterpret_cast<dla_network_desc*>(tb.addr0);

    std::cout << "[dumpOneTask] Network descriptor:\n";
    std::cout << "  operation_desc_index   = " << net->operation_desc_index   << "\n";
    std::cout << "  surface_desc_index     = " << net->surface_desc_index     << "\n";
    std::cout << "  dependency_graph_index = " << net->dependency_graph_index << "\n";
    std::cout << "  lut_data_index         = " << net->lut_data_index         << "\n";
    std::cout << "  roi_array_index        = " << net->roi_array_index        << "\n";
    std::cout << "  surface_index          = " << net->surface_index          << "\n";
    std::cout << "  stat_list_index        = " << net->stat_list_index        << "\n";
    std::cout << "  num_rois               = " << net->num_rois               << "\n";
    std::cout << "  num_operations         = " << net->num_operations         << "\n";
    std::cout << "  num_luts               = " << net->num_luts               << "\n";
    std::cout << "  num_addresses          = " << net->num_addresses          << "\n";
    std::cout << "  input_layer            = " << net->input_layer            << "\n";
    std::cout << "  dynamic_roi            = " << (int)net->dynamic_roi       << "\n";

    std::cout << "  op_head:\n";
    for (int i = 0; i < DLA_OP_NUM; ++i) {
        std::cout << "    [" << i << "] (" << opTypeToStr(i)
                  << ") = " << net->op_head[i] << "\n";
    }

    uint16_t num_ops  = net->num_operations;
    uint16_t num_luts = net->num_luts;

    // Dep graph
    dla_common_op_desc* deps = nullptr;
    if (tb.dep_graph) {
        deps = reinterpret_cast<dla_common_op_desc*>(tb.dep_graph);
        std::cout << "[dumpOneTask] dep_graph present, entry_size="
                  << sizeof(dla_common_op_desc) << "\n";
    } else {
        std::cout << "[dumpOneTask] dep_graph missing\n";
    }

    // Op list
    dla_operation_container* ops = nullptr;
    if (tb.op_list) {
        ops = reinterpret_cast<dla_operation_container*>(tb.op_list);
        std::cout << "[dumpOneTask] op_list present, entry_size="
                  << sizeof(dla_operation_container) << "\n";
    } else {
        std::cout << "[dumpOneTask] op_list missing\n";
    }

    // Surf list
    dla_surface_container* surfs = nullptr;
    if (tb.surf_list) {
        surfs = reinterpret_cast<dla_surface_container*>(tb.surf_list);
        std::cout << "[dumpOneTask] surf_list present, entry_size="
                  << sizeof(dla_surface_container) << "\n";
    } else {
        std::cout << "[dumpOneTask] surf_list missing\n";
    }

    // LUT list
    dla_lut_param* luts = nullptr;
    if (tb.lut_list && num_luts > 0) {
        luts = reinterpret_cast<dla_lut_param*>(tb.lut_list);
        std::cout << "[dumpOneTask] lut_list present, num_luts=" << num_luts
                  << " entry_size=" << sizeof(dla_lut_param) << "\n";
    } else if (num_luts > 0) {
        std::cout << "[dumpOneTask] lut_list missing but num_luts=" << num_luts << "\n";
    }

    if (!(deps && ops && surfs)) {
        std::cout << "[dumpOneTask] missing dep/op/surf, cannot fully dump operations\n";
        return;
    }

    std::cout << "\n[dumpOneTask] Operations (" << num_ops << "):\n";

    for (uint16_t i = 0; i < num_ops; ++i) {
        const dla_common_op_desc& d = deps[i];
        uint8_t op_type = d.op_type;

        std::cout << "\n  === OP " << i << " (" << opTypeToStr(op_type) << ") ===\n";
        std::cout << "    dep.index            = " << d.index          << "\n";
        std::cout << "    dep.roi_index        = " << (int)d.roi_index << "\n";
        std::cout << "    dep.op_type          = " << (int)d.op_type   << "\n";
        std::cout << "    dep.dependency_count = " << (int)d.dependency_count << "\n";

        // consumers
        std::cout << "    consumers:\n";
        for (int c = 0; c < DLA_OP_NUM; ++c) {
            const dla_consumer& cons = d.consumers[c];
            if (cons.index >= 0) {
                std::cout << "      [" << c << "] index=" << cons.index
                          << " event=" << (int)cons.event << "\n";
            }
        }
        std::cout << "    fused_parent: index=" << d.fused_parent.index
                  << " event=" << (int)d.fused_parent.event << "\n";

        const dla_operation_container& oc = ops[i];
        const dla_surface_container&   sc = surfs[i];

        // κοινό helper για όλα τα data cubes
        auto dumpCube = [](const char* tag, const dla_data_cube& c) {
            std::cout << "    [" << tag << "]\n";
            std::cout << "      type        = " << c.type << "\n";
            std::cout << "      addr_index  = " << c.address << "\n";
            std::cout << "      offset      = " << c.offset << "\n";
            std::cout << "      size        = " << c.size << "\n";
            std::cout << "      WxHxC       = " << c.width << "x"
                      << c.height << "x" << c.channel << "\n";
            std::cout << "      line_stride = " << c.line_stride << "\n";
            std::cout << "      surf_stride = " << c.surf_stride << "\n";
            std::cout << "      plane_stride= " << c.plane_stride << "\n";
        };

        switch (op_type) {
        case DLA_OP_BDMA: {
            const dla_bdma_op_desc&     bdma = oc.bdma_op;
            const dla_bdma_surface_desc& bs  = sc.bdma_surface;

            std::cout << "    [BDMA op_desc]\n";
            std::cout << "      num_transfers = " << bdma.num_transfers << "\n";

            std::cout << "    [BDMA surface]\n";
            std::cout << "      source_type      = " << (int)bs.source_type << "\n";
            std::cout << "      destination_type = " << (int)bs.destination_type << "\n";
            std::cout << "      num_transfers    = " << bs.num_transfers << "\n";

            uint16_t nt = std::min<uint16_t>(bs.num_transfers, NUM_MAX_BDMA_OPS);
            //nt = std::min<uint16_t>(nt, 4); // για να μη σε πνίξει στο log, δείξε max 4

            for (uint16_t t = 0; t < nt; ++t) {
                const dla_bdma_transfer_desc& tr = bs.transfers[t];
                std::cout << "      transfer[" << t << "]:\n";
                std::cout << "        src_addr_idx  = " << tr.source_address << "\n";
                std::cout << "        dst_addr_idx  = " << tr.destination_address << "\n";
                std::cout << "        line_size     = " << tr.line_size << "\n";
                std::cout << "        line_repeat   = " << tr.line_repeat << "\n";
                std::cout << "        source_line   = " << tr.source_line << "\n";
                std::cout << "        dest_line     = " << tr.destination_line << "\n";
                std::cout << "        surface_repeat= " << tr.surface_repeat << "\n";
                std::cout << "        source_surface= " << tr.source_surface << "\n";
                std::cout << "        dest_surface  = " << tr.destination_surface << "\n";
            }
            break;
        }

        case DLA_OP_CONV: {
            const dla_conv_op_desc&     conv = oc.conv_op;
            const dla_conv_surface_desc& cs  = sc.conv_surface;

            std::cout << "    [CONV op_desc]\n";
            std::cout << "      mode           = " << (int)conv.conv_mode << "\n";
            std::cout << "      data_reuse     = " << (int)conv.data_reuse
                      << " weight_reuse=" << (int)conv.weight_reuse << "\n";
            std::cout << "      skip_data_rls  = " << (int)conv.skip_data_rls
                      << " skip_weight_rls=" << (int)conv.skip_weight_rls << "\n";
            std::cout << "      entry_per_slice= " << conv.entry_per_slice << "\n";
            std::cout << "      data_format    = " << (int)conv.data_format
                      << " pixel_mapping=" << (int)conv.pixel_mapping << "\n";
            std::cout << "      fetch_grain    = " << conv.fetch_grain << "\n";
            std::cout << "      batch          = " << (int)conv.batch
                      << " weight_format=" << (int)conv.weight_format << "\n";
            std::cout << "      data_bank      = " << (int)conv.data_bank
                      << " weight_bank=" << (int)conv.weight_bank << "\n";
            std::cout << "      batch_stride   = " << conv.batch_stride << "\n";
            std::cout << "      post_extension = " << (int)conv.post_extension
                      << " release=" << conv.release << "\n";
            std::cout << "      input CSC WxHxC= " << conv.input_width_csc << "x"
                      << conv.input_height_csc << "x" << conv.input_channel_csc << "\n";
            std::cout << "      kernel WxHxC   = " << conv.kernel_width_csc << "x"
                      << conv.kernel_height_csc << "x" << conv.kernel_channel_csc << "\n";
            std::cout << "      input CMAC WxH = " << conv.input_width_cmac << "x"
                      << conv.input_height_cmac << "\n";
            std::cout << "      bytes_per_kernel = " << conv.bytes_per_kernel << "\n";
            std::cout << "      conv_stride     = (" << (int)conv.conv_stride_x
                      << "," << (int)conv.conv_stride_y << ")\n";
            std::cout << "      pad (l,t,r,b)   = (" << (int)conv.pad_x_left
                      << "," << (int)conv.pad_y_top << ","
                      << (int)conv.pad_x_right << "," << (int)conv.pad_y_bottom << ")\n";
            std::cout << "      dilation (x,y)  = (" << (int)conv.dilation_x
                      << "," << (int)conv.dilation_y << ")\n";
            std::cout << "      in_precision    = " << (int)conv.in_precision
                      << " out_precision=" << (int)conv.out_precision << "\n";
            std::cout << "      pad_val         = " << conv.pad_val << "\n";
            std::cout << "      in_cvt:  scale=" << conv.in_cvt.scale
                      << " trunc=" << (int)conv.in_cvt.truncate
                      << " enable=" << (int)conv.in_cvt.enable
                      << " offset=" << conv.in_cvt.offset << "\n";
            std::cout << "      out_cvt: scale=" << conv.out_cvt.scale
                      << " trunc=" << (int)conv.out_cvt.truncate
                      << " enable=" << (int)conv.out_cvt.enable
                      << " offset=" << conv.out_cvt.offset << "\n";

            dumpCube("SRC",    cs.src_data);
            dumpCube("DST",    cs.dst_data);
            dumpCube("WEIGHT", cs.weight_data);

            std::cout << "    conv_surface: offset_u=" << cs.offset_u
                      << " in_line_uv_stride=" << cs.in_line_uv_stride << "\n";
            break;
        }

        case DLA_OP_SDP: {
            const dla_sdp_op_desc&     sdp = oc.sdp_op;
            const dla_sdp_surface_desc& ss = sc.sdp_surface;

            std::cout << "    [SDP op_desc]\n";
            std::cout << "      src_precision = " << (int)sdp.src_precision
                      << " dst_precision=" << (int)sdp.dst_precision << "\n";
            std::cout << "      lut_index     = " << sdp.lut_index << "\n";
            std::cout << "      conv_mode     = " << (int)sdp.conv_mode << "\n";
            std::cout << "      batch_num     = " << (int)sdp.batch_num
                      << " batch_stride=" << sdp.batch_stride << "\n";

            auto dumpSdpOp = [](const char* tag, const struct dla_sdp_op& o) {
                std::cout << "    [" << tag << "]\n";
                std::cout << "      enable      = " << (int)o.enable << "\n";
                std::cout << "      type        = " << (int)o.type
                          << " mode=" << (int)o.mode
                          << " act=" << (int)o.act << "\n";
                std::cout << "      alu_type    = " << (int)o.alu_type << "\n";
                std::cout << "      shift_value = " << (int)o.shift_value
                          << " truncate=" << (int)o.truncate << "\n";
                std::cout << "      precision   = " << (int)o.precision << "\n";
                std::cout << "      alu_operand = " << o.alu_operand
                          << " mul_operand=" << o.mul_operand << "\n";
                std::cout << "      alu_cvt: scale=" << o.cvt.alu_cvt.scale
                          << " trunc=" << (int)o.cvt.alu_cvt.truncate
                          << " enable=" << (int)o.cvt.alu_cvt.enable
                          << " offset=" << o.cvt.alu_cvt.offset << "\n";
                std::cout << "      mul_cvt: scale=" << o.cvt.mul_cvt.scale
                          << " trunc=" << (int)o.cvt.mul_cvt.truncate
                          << " enable=" << (int)o.cvt.mul_cvt.enable
                          << " offset=" << o.cvt.mul_cvt.offset << "\n";
            };

            dumpSdpOp("X1_OP", sdp.x1_op);
            dumpSdpOp("X2_OP", sdp.x2_op);
            dumpSdpOp("Y_OP",  sdp.y_op);

            dumpCube("SRC", ss.src_data);
            dumpCube("X1",  ss.x1_data);
            dumpCube("X2",  ss.x2_data);
            dumpCube("Y",   ss.y_data);
            dumpCube("DST", ss.dst_data);
            break;
        }

        case DLA_OP_PDP: {
            const dla_pdp_op_desc&     pdp = oc.pdp_op;
            const dla_pdp_surface_desc& ps = sc.pdp_surface;

            std::cout << "    [PDP op_desc]\n";
            std::cout << "      pool_mode    = " << (int)pdp.pool_mode
                      << " pool_width=" << (int)pdp.pool_width
                      << " pool_height=" << (int)pdp.pool_height << "\n";
            std::cout << "      split_num    = " << (int)pdp.split_num << "\n";
            std::cout << "      stride_x/y   = (" << (int)pdp.stride_x
                      << "," << (int)pdp.stride_y << ")\n";
            std::cout << "      pad (l,r,t,b)= (" << (int)pdp.pad_left
                      << "," << (int)pdp.pad_right << ","
                      << (int)pdp.pad_top << "," << (int)pdp.pad_bottom << ")\n";
            std::cout << "      precision    = " << (int)pdp.precision << "\n";

            dumpCube("SRC", ps.src_data);
            dumpCube("DST", ps.dst_data);
            break;
        }

        case DLA_OP_CDP: {
            const dla_cdp_op_desc&     cdp = oc.cdp_op;
            const dla_cdp_surface_desc& csd = sc.cdp_surface;

            std::cout << "    [CDP op_desc]\n";
            std::cout << "      in_precision  = " << (int)cdp.in_precision
                      << " out_precision=" << (int)cdp.out_precision << "\n";
            std::cout << "      lut_index     = " << cdp.lut_index << "\n";
            std::cout << "      local_size    = " << (int)cdp.local_size << "\n";
            std::cout << "      bypass_sqsum  = " << (int)cdp.bypass_sqsum
                      << " bypass_out_mul=" << (int)cdp.bypass_out_mul << "\n";

            std::cout << "      in_cvt:  scale=" << cdp.in_cvt.scale
                      << " trunc=" << (int)cdp.in_cvt.truncate
                      << " enable=" << (int)cdp.in_cvt.enable
                      << " offset=" << cdp.in_cvt.offset << "\n";
            std::cout << "      out_cvt: scale=" << cdp.out_cvt.scale
                      << " trunc=" << (int)cdp.out_cvt.truncate
                      << " enable=" << (int)cdp.out_cvt.enable
                      << " offset=" << cdp.out_cvt.offset << "\n";

            dumpCube("SRC", csd.src_data);
            dumpCube("DST", csd.dst_data);
            break;
        }

        case DLA_OP_RUBIK: {
            const dla_rubik_op_desc&     rb = oc.rubik_op;
            const dla_rubik_surface_desc& rs = sc.rubik_surface;

            std::cout << "    [RUBIK op_desc]\n";
            std::cout << "      mode      = " << (int)rb.mode << "\n";
            std::cout << "      precision = " << (int)rb.precision << "\n";
            std::cout << "      stride_x/y= (" << (int)rb.stride_x
                      << "," << (int)rb.stride_y << ")\n";

            dumpCube("SRC", rs.src_data);
            dumpCube("DST", rs.dst_data);
            break;
        }

        default:
            std::cout << "    [info] unknown op_type=" << (int)op_type
                      << " (no detailed dump implemented)\n";
            break;
        }
    }

    if (luts && num_luts > 0) {
        std::cout << "\n[dumpOneTask] LUTs (" << num_luts << "): showing first only\n";
        const dla_lut_param& lp = luts[0];
        std::cout << "  LUT[0] method=" << (int)lp.method
                  << " hybrid_priority=" << (int)lp.hybrid_priority
                  << " underflow_priority=" << (int)lp.underflow_priority
                  << " overflow_priority=" << (int)lp.overflow_priority << "\n";
        std::cout << "  linear_exp_table[0]   = " << lp.linear_exp_table[0] << "\n";
        std::cout << "  linear_only_table[0]  = " << lp.linear_only_table[0] << "\n";
    }
}

void Runtime::dumpAllTensorBlobs(std::size_t maxBytesPerBlob )
{
    std::cout << "\n================ TENSOR BLOBS (tb-XX) DUMP ================\n";

    if (m_tensorBlobs.empty()) {
        std::cout << "[dumpAllTensorBlobs] no tensor blobs recorded\n";
        return;
    }

    for (const auto &kv : m_tensorBlobs) {
        const std::string &name = kv.first;   // "tb-0", "tb-1", ...
        const TensorBlob &tb    = kv.second;

        std::cout << "\n--- Tensor '" << name << "' ---\n";
        std::cout << "  data = " << static_cast<const void*>(tb.data)
                  << " size = " << tb.size << " bytes\n";

        if (!tb.data || tb.size == 0) {
            std::cout << "  [empty]\n";
            continue;
        }

        std::size_t toDump = std::min<std::size_t>(tb.size, maxBytesPerBlob);
        std::cout << "  first " << toDump << " bytes (hex):\n    ";

        /*
        for (std::size_t i = 0; i < toDump; ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << (unsigned int)tb.data[i] << " ";

            if ((i + 1) % 16 == 0 && i + 1 < toDump) {
                std::cout << "\n    ";
            }
        }
        std::cout << std::dec << "\n";
        */
    }

    std::cout << "\n================ END TENSOR BLOBS DUMP ====================\n";
}

NvDlaError Runtime::loadMemory(Loadable *l, Memory *memory)
{
    NvDlaError e = NvDlaSuccess;
    NvU8 *mem;
    NVDLA_UNUSED(mem);

    std::cout << "\n[Runtime::loadMemory] >>> ENTER\n";
    std::cout << "[Runtime::loadMemory] memory id=" << memory->id()
              << " size=" << memory->size()
              << " flags="<<(int) memory->flags()
              << " domain=" << (int) memory->domain()
              << " bindable=" << memory->bindable()
              << "\n";

    bool ok = false;
    if ( ! (memory->flags() & ILoadable::MemoryListEntry::flags_alloc()) ) {
        std::cout << "[Runtime::loadMemory] flags_alloc() NOT set -> nothing to allocate/load, return NvDlaSuccess\n";
        return NvDlaSuccess;
    }

    // skip top-level, bindable buffers.  they are dealt with explicitly elsewhere
    if (  memory->bindable() ) {
        std::cout << "[Runtime::loadMemory] memory is bindable -> handled elsewhere, return NvDlaSuccess\n";
        return NvDlaSuccess;
    }

    if ( memory->domain() == ILoadable::MemoryListEntry::domain_sysmem() )
    {
        std::cout << "[Runtime::loadMemory] domain_sysmem(): will allocate / map system memory\n";

        void *mapped_mem = NULL;

        NvU64 size = memory->size();
        void *hDla = getDLADeviceContext(m_loaded_instance);
        void *hMem = memory->getHandle();

        std::cout << "[Runtime::loadMemory] before alloc/map: hMem=" << hMem
                  << " size=" << size << " hDLa " << hDla << " and m_dla_hanndle "<< m_dla_handle << "\n";

        std::cout << "[Runtime::loadMemory] before alloc/map: &hMem=" << &hMem << "\n" ;

        if (hMem == 0) {
            /* Allocate memory for network */
            std::cout << "[Runtime::loadMemory] hMem == 0 (/* Allocate memory for network */) -> NvDlaAllocMem\n";
            PROPAGATE_ERROR_FAIL( NvDlaAllocMem(m_dla_handle, hDla, &hMem, (void **)(&mapped_mem), size, NvDlaHeap_System) );
            mapped_mem = staticAlloc(size);

            std::cout << "[Runtime::loadMemory] NvDlaAllocMem OK, hMem=" << hMem
                      << " mapped_mem=" << mapped_mem << "\n";

            memory->setHandle(hMem);
            memory->setVirtAddr(mapped_mem);
        }
        else {
            mapped_mem = memory->getVirtAddr();
            std::cout << "[Runtime::loadMemory] hMem already valid, mapped_mem="
                      << mapped_mem << "\n";
        }

        if ( memory->flags() & ILoadable::MemoryListEntry::flags_set() )
        {
            std::cout << "[Runtime::loadMemory] flags_set() is set -> will copy contents into mapped_mem\n";

            if ( memory->contents().size() != memory->offsets().size() ) {
                std::cout << "[Runtime::loadMemory] ERROR: contents.size()="
                          << memory->contents().size()
                          << " != offsets.size()=" << memory->offsets().size()
                          << "\n";
                ORIGINATE_ERROR_FAIL(NvDlaError_InvalidState,
                                     "mismatch on num content blobs vs. num offsets in memory id");
            }
            vector<string> &contents = memory->contents();
            vector<uint64_t> &offsets  = memory->offsets();

            std::cout << "[Runtime::loadMemory] contents.size()=" << contents.size()
                      << " offsets.size()=" << offsets.size() << "\n";

            for ( size_t ci = 0, CI = contents.size(); ci != CI; ++ci )
            {
                ILoadable::Blob content_blob;
                NvU8 *data;

                const string &content_symbol = contents[ci];

                std::cout << "\n[Runtime::loadMemory]   >> content index ci=" << ci
                          << " symbol='" << content_symbol << "'"
                          << " offset=" << offsets[ci] << "\n";

                ok = l->getSymbolContent(content_symbol, content_blob, data);
                std::cout << "[Runtime::loadMemory]   getSymbolContent() -> ok=" << ok
                          << " name=" << content_blob.name
                          << " size=" << content_blob.size
                          << " data=" << static_cast<void*>(data) << "\n";
                if ( !ok ) {
                    std::cout << "[Runtime::loadMemory]   ERROR: failed to find buffer content symbol '"
                              << content_symbol << "'\n";
                    ORIGINATE_ERROR_FAIL(NvDlaError_InvalidState,
                                         "failed to find buffer content symbol %s",
                                         content_symbol.c_str());
                }

                if (ok && data && content_blob.size > 0) {
                    /*
                    for (int i =0; i < (int)content_blob.size; i++) {
                        std::cout << "[Runtime::loadMemory]     data[" << i << "] = 0x"
                                  << std::hex << (unsigned int)(data[i]) << std::dec << "\n";
                    }
                    */
                }

                if ( memory->size() >= (NvU64)(offsets[ci] + content_blob.size) )
                {
                    NvU8 *src = data;
                    NvU8 *dst = (NvU8*)mapped_mem + offsets[ci];

                    std::cout << "[Runtime::loadMemory]   copy blob: dst_base=" << mapped_mem
                              << " dst=" << static_cast<void*>(dst)
                              << " src=" << static_cast<void*>(src)
                              << " blob_size=" << content_blob.size << "\n";

                    for ( size_t byte = 0; byte < content_blob.size; byte++ ) {
                        dst[byte] = src[byte];
                    }
                    std::cout << "[Runtime::loadMemory]   copy done for ci=" << ci << "\n";

                    // my code
                    std::string base  = content_symbol;
                    std::string kind;
                    bool done = false;
                    size_t pos = content_symbol.rfind('-');
                    if (pos != std::string::npos) {
                        base = content_symbol.substr(0, pos);      // "task-0"
                        kind = content_symbol.substr(pos + 1);     // "addr0", "dep_graph", ...
                    }

                    std::cout << "[Runtime::loadMemory]   content_symbol='" << content_symbol
                              << "' parsed to base='" << base
                              << "' kind='" << kind << "'\n";
                    
                    TaskBlobs &tb = m_taskBlobs[base];
                    
                    if (kind == "addr0") {
                        tb.addr0      = dst;               
                        tb.addr0_size = content_blob.size;
                        done = true;
                    } else if (kind == "dep_graph") {
                        tb.dep_graph  = dst;
                        tb.dep_size   = content_blob.size;
                        done = true;
                    } else if (kind == "op_list") {
                        tb.op_list    = dst;
                        tb.op_size    = content_blob.size;
                        done = true;
                    } else if (kind == "surf_list") {
                        tb.surf_list  = dst;
                        tb.surf_size  = content_blob.size;
                        done = true;
                    } else if (kind == "lut_list") {
                        tb.lut_list   = dst;
                        tb.lut_size   = content_blob.size;
                        done = true;
                    }
                    
                    if(done)
                        std::cout << "[Runtime::loadMemory]   recorded blob for base='"
                                  << base << "' kind='" << kind
                                  << "' size=" << content_blob.size
                                  << " dst=" << static_cast<void*>(dst) << "\n";
                    
                    if (content_symbol.rfind("tb-", 0) == 0) {  // starts with "tb-"
                        TensorBlob &t = m_tensorBlobs[content_symbol];
                        t.data = dst;
                        t.size = content_blob.size;

                        std::cout << "[Runtime::loadMemory]   recorded tensor blob '"
                                  << content_symbol << "' size=" << content_blob.size
                                  << " dst=" << static_cast<void*>(dst) << "\n";
                    }
                   // end of my code                     
                }
                else {
                    std::cout << "[Runtime::loadMemory]   ERROR: content blob too large for pool size\n"
                              << "    memory->size()=" << memory->size()
                              << " required=" << (offsets[ci] + content_blob.size) << "\n";
                    ORIGINATE_ERROR_FAIL(NvDlaError_InvalidState, "content blob too large for pool size");
                }
            }
        } else {
            std::cout << "[Runtime::loadMemory] flags_set() NOT set -> no contents copied\n";
        }
    } else {
        std::cout << "[Runtime::loadMemory] domain != sysmem, domain=" << memory->domain()
                  << " -> nothing done here\n";
    }

 fail:
    if (e != NvDlaSuccess) {
        std::cout << "[Runtime::loadMemory] <<< EXIT with ERROR e=" << e << "\n";
    } else {
        std::cout << "[Runtime::loadMemory] <<< EXIT (success)\n";
    }
    return e;
}


NvDlaError Runtime::getNetworkDataType(DataType::UnderlyingType * /*data_type*/) const
{
    NvDlaError e = NvDlaSuccess;
    return e;
}

//
// create arrays of {input, output} X {id-ordered bind ids} -> Memory objects
// check for duplicates and other sorts of malformed-ness.
//
NvDlaError Runtime::initBindableMemory()
{
    NvDlaError e = NvDlaSuccess;

    std::cout << "[Runtime::initBindableMemory] >>> ENTER\n";
    std::cout << "[Runtime::initBindableMemory] m_tensor_desc_entries.size() = "
              << m_tensor_desc_entries.size() << "\n";
    std::cout << "[Runtime::initBindableMemory] m_memory.size()              = "
              << m_memory.size() << "\n";

    m_tensor_desc.resize(m_tensor_desc_entries.size());

    std::cout << "[Runtime::initBindableMemory] ILoadable m_tensor_desc_entries.size()   = " << m_tensor_desc_entries.size() << "\n";
    std::cout << "[Runtime::initBindableMemory] ILoadable m_tensor_desc_entries.data()   = " << (const void*)m_tensor_desc_entries.data() << "\n";


    std::cout << "[Runtime::initBindableMemory] runtime m_tensor_desc.size()   = " << m_tensor_desc.size() << "\n";
    std::cout << "[Runtime::initBindableMemory] runtime m_tensor_desc.data()   = " << (const void*)m_tensor_desc.data() << "\n";


    for ( size_t tdi = 0, TDI = m_tensor_desc_entries.size(); tdi != TDI; ++tdi ) {
        std::cout << "[Runtime::initBindableMemory] TensorDesc init: tdi = " << tdi << "\n";

         std::cout << "  m_tensor_desc_entries[" << tdi << "]: "
                   << " name=" << m_tensor_desc_entries[tdi].name
                   << " id=" << m_tensor_desc_entries[tdi].id
                   << " memId=" << m_tensor_desc_entries[tdi].memId
                   << " size=" << m_tensor_desc_entries[tdi].size
                   << " offset=" << m_tensor_desc_entries[tdi].offset
                   << " dims=[" << m_tensor_desc_entries[tdi].dims.n
                   << "," << m_tensor_desc_entries[tdi].dims.c
                   << "," << m_tensor_desc_entries[tdi].dims.h
                   << "," << m_tensor_desc_entries[tdi].dims.w << "]"
                   << " dataFormat=" << (int)m_tensor_desc_entries[tdi].dataFormat
                   << " dataType=" << (int)m_tensor_desc_entries[tdi].dataType
                   << " dataCategory=" << (int)m_tensor_desc_entries[tdi].dataCategory
                   << " pixelFormat=" << (int)m_tensor_desc_entries[tdi].pixelFormat
                   << " pixelMapping=" << (int)m_tensor_desc_entries[tdi].pixelMapping
                   << " stride=["
                   << m_tensor_desc_entries[tdi].stride[0] << ","
                   << m_tensor_desc_entries[tdi].stride[1] << ","
                   << m_tensor_desc_entries[tdi].stride[2] << ","
                   << m_tensor_desc_entries[tdi].stride[3] << "]"
                   << "\n";


        m_tensor_desc[tdi] = TensorDesc(m_tensor_desc_entries[tdi]);
        if ( m_tensor_desc[tdi].id != tdi ) {
            gLogInfo << "tdi != TDI " << tdi << " " << m_tensor_desc[tdi].id << endl;
            std::cout << "  WARNING: TensorDesc id mismatch: tdi=" << tdi
                      << " id=" << m_tensor_desc[tdi].id << "\n";
        }
    }

    std::cout << "[Runtime::initBindableMemory] Resize m_bindable_memory to IOD_Max = "
              << IOD_Max << "\n";
    m_bindable_memory.resize(IOD_Max);

    std::cout << "[Runtime::initBindableMemory] Scanning m_memory for bindable regions...\n";

    for ( size_t mi = 0, MI = m_memory.size(); mi != MI; ++mi )
    {
        IOD which_iod;
        int bind_id;

        std::cout << "[Runtime::initBindableMemory] Memory index mi = " << mi << "\n";

        bind_id = m_memory[mi].bindId(which_iod);
        std::cout << "  bindId() returned: bind_id = " << bind_id
                  << " , which_iod = " << (int)which_iod << "\n";        
        if ( bind_id == -1 ) {
            std::cout << "  --> Not bindable (bind_id == -1), continue.\n";
            continue;
        }

        // insert and detect any duplicates
        {
            std::vector<Memory *> &which_mem = m_bindable_memory[which_iod];
            MemoryId_BindId_Is check_for_dup_id(bind_id);

            std::cout << "  m_bindable_memory[" << (int)which_iod << "].size() before = "
                      << which_mem.size() << "\n";

            std::cout << " &m_bindable_memory["<<  which_iod << "] = " << (void*)&m_bindable_memory[which_iod] << "\n"; 

            for(int i=0; i < which_mem.size(); i++) {
                std::cout << " which_mem[" << i << "] = " << which_mem[i] << "\n";
                std::cout << "   bindId = " << which_mem[i]->bindId(which_iod) << "\n";
                std::cout << "   Memory id = " << which_mem[i]->id() << "\n";
                std::cout << "   Memory handle = " << which_mem[i]->getHandle() << "\n";
                std::cout << "   Memory virtAddr = " << which_mem[i]->getVirtAddr() << "\n";
                std::cout << "   Memory size = " << which_mem[i]->size() << "\n";
                std::cout << "   Memory domain = " << which_mem[i]->domain() << "\n";
                std::cout << "   Memory flags = " << (int)which_mem[i]->flags() << "\n";
                std::cout << "\n";
            }
            std::cout << "  Checking for duplicate bind_id = " << bind_id << "...\n";

            if ( which_mem.end() == std::find_if(which_mem.begin(), which_mem.end(), check_for_dup_id) ) {
                std::cout << "  No duplicate found. Pushing Memory* for mi=" << mi << "\n";
                which_mem.push_back(&m_memory[mi]);
                std::cout << "  m_bindable_memory[" << (int)which_iod << "].size() now = "
                          << which_mem.size() << "\n";
            } else {
                std::cout << "  ERROR: Duplicate bind id " << bind_id
                          << " on separate memory objects in runtime.\n";
                ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "Duplicate bind ids on separate memory objects in runtime.");
            }
        }
    }

    // it's possible that these we're given out of order.
    // sort now based upon bind id in each category.
    {
        Memory_BindId_LT_Compare less_than;
        IOD w;
        NVDLA_UNUSED(w);

        std::cout << "[Runtime::initBindableMemory] Sorting m_bindable_memory by bindId...\n";

        for ( size_t w = 0; w < size_t(IOD_Max); w++ )
        {
            int num_ids = int(m_bindable_memory[w].size());

            std::cout << "[Runtime::initBindableMemory] IOD w = " << w
                      << " -> num_ids = " << num_ids << "\n";

            if ( ! m_bindable_memory[w].size() ) {
                std::cout << "  No bindable memories for this IOD, continue.\n";
                continue;
            }

            std::cout << "  Before sort, bindIds: ";
            for (int i = 0; i < num_ids; ++i) {
                IOD tmp;
                int bid = m_bindable_memory[w][i]->bindId(tmp);
                std::cout << bid << (i+1 < num_ids ? ", " : "");
            }
            std::cout << "\n";

            std::sort(m_bindable_memory[w].begin(),  m_bindable_memory[w].end(), less_than);

            std::cout << "  After sort, bindIds:  ";
            for (int i = 0; i < num_ids; ++i) {
                IOD tmp;
                int bid = m_bindable_memory[w][i]->bindId(tmp);
                std::cout << bid << (i+1 < num_ids ? ", " : "");
            }
            std::cout << "\n";

            // now check to be sure there are no gaps/out-of-bounds ids
            IOD na;
            int first_id = m_bindable_memory[w][0]->bindId(na);
            int last_id  = m_bindable_memory[w][num_ids-1]->bindId(na);

            std::cout << "  first_id = " << first_id
                      << ", last_id = " << last_id
                      << ", expected last = " << (num_ids - 1) << "\n";

            if ( (first_id != 0) || (last_id != (num_ids-1)) ) {
                std::cout << "  ERROR: Out of bounds bind id on memory object\n";
                ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "Out of bounds bind id on memory object");
            } else {
                std::cout << "  BindId range OK [0.." << (num_ids-1) << "]\n";
            }
        }
    }


    for(int loop=0; loop < m_bindable_memory.size(); loop++) {
        std::cout << "[Runtime::initBindableMemory] Final bindable_memory[" << loop << "] has "
                  << m_bindable_memory[loop].size() << " entries.\n";

        for(int p=0; p < m_bindable_memory[loop].size(); p++) {
            Memory *mem = m_bindable_memory[loop][p];
            IOD tmp;
            int bid = mem->bindId(tmp);
            std::cout << "  Entry " << p << ": bindId=" << bid
                      << " memId=" << mem->id()
                      << " handle=" << mem->getHandle()
                      << " virtAddr=" << mem->getVirtAddr()
                      << " size=" << mem->size()
                      << "\n";
        }
    }

    std::cout << "[Runtime::initBindableMemory] <<< EXIT (success)\n";

 fail:
    if (e != NvDlaSuccess) {
        std::cout << "[Runtime::initBindableMemory] <<< EXIT with ERROR e=" << e << "\n";
    }
    return e;
}

NvDlaError Runtime::getNumInputTensors(int *inputs)
{
    NvDlaError e = NvDlaSuccess;
    int input_id = 0;
    NVDLA_UNUSED(input_id);
    if ( !inputs )
    {
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter);
    }
    *inputs = m_bindable_memory[IOD_Input].size();

 fail:
    return e;
}

NvDlaError Runtime::getNumOutputTensors(int *outputs)
{
    NvDlaError e = NvDlaSuccess;
    if ( !outputs )
    {
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter);
    }
    *outputs = m_bindable_memory[IOD_Output].size();
 fail:
    return e;
}

NvDlaError Runtime::getMemoryFromBindId(IOD w, int id, Memory * &bound_mem)
{
    NvDlaError e = NvDlaSuccess;
    if ( (id < 0) || (size_t(id) >= m_bindable_memory[w].size()) ) {
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "Bind id out of range:%d", id);
    }

    bound_mem = m_bindable_memory[w][id];
 fail:
    return e;
}

IRuntime::NvDlaTensor Runtime::TensorDesc::bindTensorDesc() const
{
    IRuntime::NvDlaTensor td;

    int srcBytes = snprintf(td.name, sizeof(td.name), "%s", name.c_str());
    if ((srcBytes < 0) || (static_cast<size_t>(srcBytes) >= sizeof(td.name)))
        REPORT_ERROR(NvDlaError_BadValue, "String truncation encountered, \"%s\" -> \"%s\"", name.c_str(), td.name);

    td.bufferSize = size;
    td.dims = dims;
    td.dataFormat = dataFormat;
    td.dataType = dataType;
    td.dataCategory = dataCategory;
    td.pixelFormat = pixelFormat;
    td.pixelMapping = pixelMapping;
    for ( size_t i = 0; i < NVDLA_RUNTIME_TENSOR_DESC_NUM_STRIDES; ++i )
    {
        td.stride[i] = stride[i];
    }
    return td;
}

NvDlaError Runtime::getInputTensorDesc(int id, IRuntime::NvDlaTensor *td)
{
    NvDlaError e = NvDlaSuccess;
    Memory *bound_mem = 0;
    int tensor_desc_id = -1;

    if ( !td )
    {
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter);
    }

    PROPAGATE_ERROR_FAIL( getMemoryFromBindId(IOD_Input, id, bound_mem) );

    tensor_desc_id = bound_mem->tensorDescId();
    if ( (tensor_desc_id < 0) || (size_t(tensor_desc_id) >= m_tensor_desc.size()) ) {
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "Tensor desc id out of range:%d", tensor_desc_id);
    }

    *td = m_tensor_desc[tensor_desc_id].bindTensorDesc();

 fail:
    return e;
}

NvDlaError Runtime::getOutputTensorDesc(int id, IRuntime::NvDlaTensor *td)
{
    NvDlaError e = NvDlaSuccess;
    Memory *bound_mem = 0;
    int tensor_desc_id = -1;

    if ( !td )
    {
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter);
    }

    PROPAGATE_ERROR_FAIL( getMemoryFromBindId(IOD_Output, id, bound_mem) );

    tensor_desc_id = bound_mem->tensorDescId();
    if ( (tensor_desc_id < 0) || (size_t(tensor_desc_id) >= m_tensor_desc.size()) ) {
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "Tensor desc id out of range:%d", tensor_desc_id);
    }

    *td = m_tensor_desc[tensor_desc_id].bindTensorDesc();

 fail:
    return e;
}

//
// take a tensor descriptor from the user-facing API and inspect the
// changed elements.  react to those which can be legitimately tweaked
// (.e.g line_stride) and complain about any which cannot.
//
NvDlaError Runtime::mergeSetTensorDesc(IOD iod, int bindId, int tensorDescId, const IRuntime::NvDlaTensor *tdl)
{
    Runtime::TensorDesc *origEntry = 0;
    const NvU32 *newStrides = 0;
    NvU32 *oldStrides = 0;
    bool nameDiff = false;
    bool stridesDiff = false, dimsDiff; //, sizeDiff;
    bool dataFormatDiff, dataTypeDiff, dataCategoryDiff;
    bool pixelFormatDiff, pixelMappingDiff;

    NvDlaError e = NvDlaSuccess;

    if ( (tensorDescId < 0) || (size_t(tensorDescId) >= m_tensor_desc.size()) )
    {
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "Tensor desc id out of range:%d", tensorDescId);
    }

    origEntry = &m_tensor_desc[tensorDescId];

    newStrides = &tdl->stride[0];
    oldStrides = &(origEntry->stride[0]);

    nameDiff = (std::strncmp(tdl->name, origEntry->name.c_str(), sizeof(tdl->name)) != 0);

    stridesDiff = false;

    dimsDiff = ( (origEntry->dims.n != tdl->dims.n) ||
                 (origEntry->dims.c != tdl->dims.c) ||
                 (origEntry->dims.h != tdl->dims.h) ||
                 (origEntry->dims.w != tdl->dims.w) );

    for ( size_t ss = 0; ss < NVDLA_RUNTIME_TENSOR_DESC_NUM_STRIDES; ++ss )
    {
        // assume the worst for now.  avoid missing a set which was actually needed.
        stridesDiff = true; // stridesDiff || (oldStrides[ss] != newStrides[ss]);
    }

    dataFormatDiff   = origEntry->dataFormat   != tdl->dataFormat;
    dataTypeDiff     = origEntry->dataType     != tdl->dataType;
    dataCategoryDiff = origEntry->dataCategory != tdl->dataCategory;

    pixelFormatDiff  = origEntry->pixelFormat  != tdl->pixelFormat;
    pixelMappingDiff = origEntry->pixelMapping != tdl->pixelMapping;

    // Name changes are not supported
    if ( nameDiff )
    {
        ORIGINATE_ERROR_FAIL(NvDlaError_NotSupported, "name change requested");
    }

    // at the moment no formatting changes are allowed.  eventually we may be
    // in a position to do inline or hand-off formatting operations.
    if ( dataFormatDiff || dataTypeDiff || dataCategoryDiff )
    {
        ORIGINATE_ERROR_FAIL(NvDlaError_NotImplemented, "data format/type/category change requested");
    }

    // pixel format and mapping changes are unlikely to be allowed
    if ( pixelFormatDiff || pixelMappingDiff )
    {
        ORIGINATE_ERROR_FAIL(NvDlaError_NotSupported, "pixel format/mapping change requested");
    }

    if ( dimsDiff )
    {
        ORIGINATE_ERROR_FAIL(NvDlaError_NotSupported, "dimensions change requested");
    }

    if ( stridesDiff )
    {
        // the way this works, whatever was written last is what will be set thereafter.
        // even if extra work is required.  it's sticky.
        for ( size_t ss = 0; ss < NVDLA_RUNTIME_TENSOR_DESC_NUM_STRIDES; ++ss )
        {
            oldStrides[ss] = newStrides[ss];
        }

        PROPAGATE_ERROR_FAIL( rewriteStrides(iod, bindId, tensorDescId, newStrides) );
    }

 fail:
    return e;
}

NvDlaError Runtime::setInputTensorDesc(int bindId, const IRuntime::NvDlaTensor *td)
{
    NvDlaError e = NvDlaSuccess;
    Memory *boundMem = 0;
    int tensorDescId = -1;

    if ( !td )
    {
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter);
    }

    PROPAGATE_ERROR_FAIL( getMemoryFromBindId(IOD_Input, bindId, boundMem) );

    tensorDescId = boundMem->tensorDescId();
    if ( (tensorDescId < 0) || (size_t(tensorDescId) >= m_tensor_desc.size()) )
    {
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "Tensor desc id out of range:%d", tensorDescId);
    }

    PROPAGATE_ERROR_FAIL( mergeSetTensorDesc(IOD_Input, bindId, tensorDescId, td) );

 fail:
    return e;
}

NvDlaError Runtime::setOutputTensorDesc(int bindId, const IRuntime::NvDlaTensor *td)
{
    NvDlaError e = NvDlaSuccess;
    Memory *boundMem = 0;
    int tensorDescId = -1;

    if ( !td )
    {
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter);
    }

    PROPAGATE_ERROR_FAIL( getMemoryFromBindId(IOD_Output, bindId, boundMem) );

    tensorDescId = boundMem->tensorDescId();
    if ( (tensorDescId < 0) || (size_t(tensorDescId) > m_tensor_desc.size()) )
    {
        ORIGINATE_ERROR_FAIL(NvDlaError_BadParameter, "Tensor desc id out of range:%d", tensorDescId);
    }

    PROPAGATE_ERROR_FAIL( mergeSetTensorDesc(IOD_Output, bindId, tensorDescId, td) );

 fail:
    return e;

}

static bool skipStrideRewrite = false;

NvDlaError Runtime::rewriteStrides(IOD iod, int bindId, int tensorDescId, const NvU32 *newStrides)
{
    NvDlaError e = NvDlaSuccess;

    Memory *boundMem = 0;
    NvU16 memId;
    list<Address *> alis;


    PROPAGATE_ERROR_FAIL( getMemoryFromBindId(iod, bindId, boundMem) );

    if ( !boundMem )
    {
        ORIGINATE_ERROR_FAIL(NvDlaError_InvalidState, "missing bound mem?");
    }

    memId = boundMem->id();

    if ( debugStrideRewrite() )
    {
        gLogInfo << "rewriting strides for iod=" << (int)iod <<
            " tensor desc id=" << tensorDescId << " bind id=" << bindId << endl;
    }

    //
    // need to find all address list entries relative to this mem id.
    // not likely to be >1 of these unless multi-batch is in play.
    // with multibatch alive there can be more one address list
    // id generated per batch elem, all sharing the original bindable mem id.
    //
    for ( size_t ali = 0, ALI = m_address_entries.size(); ali != ALI; ++ali )
    {
        if ( m_address[ali].mem_id() == memId )
        {
            alis.push_back(&m_address[ali]);
        }
    }

    alis.unique();

    if ( alis.size() )
    {
        list<Address *>::iterator a;

        if ( debugStrideRewrite() )
        {
            stringstream ss;
            string delim;
            for ( a = alis.begin(); a != alis.end(); ++a )
            {
                ss << delim << (*a)->id();
                delim = ", ";
            }
            gLogInfo << "rewrite needs to update relocation entries re: address list ids={" <<
                ss.str() << "}" << endl;
        }

        for ( a = alis.begin(); a != alis.end(); ++a )
        {
            Address *addr = *a;
            for ( size_t ri = 0, RI = m_reloc_entries.size(); ri != RI; ++ri )
            {
                ILoadable::RelocEntry &re = m_reloc_entries[ri];

                if ( debugStrideRewrite() )
                {
                    gLogInfo << "consider reloc[" << ri << "].addr id=" << re.addressListId <<
                        " vs. addr id=" << addr->id() << endl;
                }

                if ( re.addressListId != addr->id() )
                {
                    continue;
                }

                if ( debugStrideRewrite() )
                {
                    gLogInfo << "\treloc[" << ri << "].interface=" << re.interface <<
                        ".subInterface=" << re.subInterface << endl;
                }

                if ( (re.interface == NVDLA_LOADABLE_INTERFACE_DLA1) ||
                     (re.interface == NVDLA_LOADABLE_INTERFACE_EMU1) )
                {
                    switch ( re.subInterface )
                    {
                        case NVDLA_LOADABLE_SUB_INTERFACE_DLA1_DEPS:
                            for ( size_t mi = 0, MI = m_memory.size(); mi != MI; ++mi )
                            {
                                if ( m_memory[mi].id() == re.writeId )
                                {
                                    ORIGINATE_ERROR_FAIL(NvDlaError_NotSupported,
                                                         "write hot deps into mem id %d",
                                                         re.writeId);
                                }
                            }
                            break;

                            /*
                              dup'd enumerants. just being clear.
                        case NVDLA_LOADABLE_SUB_INTERFACE_EMU1_OPS:
                        case NVDLA_LOADABLE_SUB_INTERFACE_EMU1_SURFS:
                            */
                        case NVDLA_LOADABLE_SUB_INTERFACE_DLA1_OPS:
                        case NVDLA_LOADABLE_SUB_INTERFACE_DLA1_SURFS:
                            for ( size_t mi = 0, MI = m_memory.size(); mi != MI; ++mi )
                            {
                                NvU32 *addr = 0;
                                NvU32 origVal;

                                if ( m_memory[mi].id() != re.writeId )
                                {
                                    continue;
                                }

                                if ( !m_memory[mi].getVirtAddr() )
                                {
                                    continue;
                                }

                                addr = (NvU32*) ((NvU32 *)m_memory[mi].getVirtAddr() + re.offset);

                                origVal = *addr;

                                if ( !skipStrideRewrite )
                                {
                                    if ( re.relocType == ELST_Line )
                                    {
                                        *addr = newStrides[1];
                                    }
                                    else if ( re.relocType == ELST_Surf )
                                    {
                                        *addr = newStrides[2];
                                    }
                                    else
                                    {
                                        ORIGINATE_ERROR(NvDlaError_InvalidState, "bogus reloc type");
                                    }
                                }

                                if ( debugStrideRewrite() && !skipStrideRewrite )
                                {
                                    gLogInfo << "wrote hot reloc interface=" << (int)re.interface <<
                                        " subInterface=" << (int)re.subInterface <<
                                        " type=" << (int)re.relocType <<
                                        " into mem id" << re.writeId << " @" <<
                                        (void*)(addr) << " + " << re.offset << " = " << addr <<
                                        " orig val="    << std::hex << origVal << std::dec <<
                                        " current val=" << std::hex << *addr   << std::dec << endl;
                                }
                            }
                            break;

                        default:
                            break;
                    }
                }
            }
        }
    }

 fail:
    return e;
}

Runtime::TensorDesc::TensorDesc()
{
    id = 0;
    memId = 0;
    size = 0;
    offset = 0;
    dims.n = 0;
    dims.c = 0;
    dims.h = 0;
    dims.w = 0;
    dataFormat = 0;
    dataType = 0;
    dataCategory = 0;
    pixelFormat = 0;
    pixelMapping = 0;
    stride[0] = 0;
    stride[1] = 0;
    stride[2] = 0;
    stride[3] = 0;
    stride[4] = 0;
    stride[5] = 0;
    stride[6] = 0;
    stride[7] = 0;
}

Runtime::TensorDesc::TensorDesc(const ILoadable::TensorDescListEntry &e)
{
    name  = e.name;
    id    = e.id;
    memId = e.memId;
    size         = e.size;
    offset       = e.offset;
    dims.n       = e.dims.n;
    dims.c       = e.dims.c;
    dims.h       = e.dims.h;
    dims.w       = e.dims.w;
    dataFormat   = e.dataFormat;
    dataType     = e.dataType;
    dataCategory = e.dataCategory;
    pixelFormat  = e.pixelFormat;
    pixelMapping = e.pixelMapping;
    stride[0] = e.stride[0];
    stride[1] = e.stride[1];
    stride[2] = e.stride[2];
    stride[3] = e.stride[3];
    stride[4] = e.stride[4];
    stride[5] = e.stride[5];
    stride[6] = e.stride[6];
    stride[7] = e.stride[7];
}

NvDlaError Runtime::setNumDLAs(NvU8 dlas)
{
    NvDlaError e = NvDlaSuccess;

    if (dlas > MAX_NUM_DLAS) {
        ORIGINATE_ERROR_FAIL(NvDlaError_InvalidSize,
            "number of dlas %d is not supported (max: %d)",
            dlas, MAX_NUM_DLAS);
    }
    num_dlas = dlas;

fail:
    return e;
}

NvU8 Runtime::getNumDLAs()
{
    return num_dlas;
}

NvDlaError Runtime::setNumBatches(NvU8 batches)
{
    NvDlaError e = NvDlaSuccess;

    if (batches > MAX_NUM_BATCHES) {
        ORIGINATE_ERROR_FAIL(NvDlaError_InvalidSize,
            "number of batches %d is not supported (max: %d)",
            batches, MAX_NUM_BATCHES);
    }
    num_batches = batches;

fail:
    return e;
}

} // nvdla::priv

} // nvdla
