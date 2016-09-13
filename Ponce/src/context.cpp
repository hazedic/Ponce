#include <cstring>
#include <stdexcept>

//Triton
#include <cpuSize.hpp>
#include <coreUtils.hpp>
#include <x86Specifications.hpp>
#include <api.hpp>
#include "context.hpp"
#include "globals.hpp"

//IDA
#include <dbg.hpp>
#include <pro.h>

//Ponce
#include "globals.hpp"

/*This callback is called when triton is processing a instruction and it needs a memory value to build the expressions*/
void needConcreteMemoryValue(triton::arch::MemoryAccess& mem)
{
	if (cmdOptions.showExtraDebugInfo)
		msg("[+] We need memory! Address: " HEX_FORMAT " Size: %u\n", (ea_t)mem.getAddress(), mem.getSize());
	auto memValue = getCurrentMemoryValue((ea_t)mem.getAddress(), mem.getSize());
	msg("Reading memory value: "HEX_FORMAT"\n", memValue.convert_to<ea_t>());
	mem.setConcreteValue(memValue);
	triton::api.setConcreteMemoryValue(mem);
}

/*This callback is called when triton is processing a instruction and it needs a regiter to build the expressions*/
void needConcreteRegisterValue(triton::arch::Register& reg)
{
	auto regValue = getCurrentRegisterValue(reg);
	if (cmdOptions.showExtraDebugInfo)
		msg("[+] We need a register! Register: %s Value: " HEX_FORMAT "\n", reg.getName().c_str(), regValue.convert_to<ea_t>());
	reg.setConcreteValue(regValue);
	triton::api.setConcreteRegisterValue(reg);
}

triton::uint512 getCurrentRegisterValue(triton::arch::Register& reg)
{
	regval_t reg_value;
	triton::uint512 value = 0;
	//We need to invalidate the registers. If not IDA uses the last value when program was stopped
	invalidate_dbg_state(DBGINV_REGS);
	get_reg_val(reg.getName().c_str(), &reg_value);
	value = reg_value.ival;
	/* Sync with the libTriton */
	triton::arch::Register syncReg;
	if (reg.getId() >= triton::arch::x86::ID_REG_AF && reg.getId() <= triton::arch::x86::ID_REG_ZF)
		syncReg = TRITON_X86_REG_EFLAGS;
	else if (reg.getId() >= triton::arch::x86::ID_REG_IE && reg.getId() <= triton::arch::x86::ID_REG_FZ)
		syncReg = TRITON_X86_REG_MXCSR;
	else
		syncReg = reg.getParent();

	syncReg.setConcreteValue(value);
	triton::api.setConcreteRegisterValue(syncReg);
	/* Returns the good casted value */
	return triton::api.getConcreteRegisterValue(reg, false);
}


triton::uint128 getCurrentMemoryValue(ea_t addr, triton::uint32 size) 
{
	if (size > 16)
	{
		warning("[!]Error, size can't be larger than 16\n");
		return -1;
	}
	triton::uint8 buffer[16] = {0};
	//This is the way to force IDA to read the value from the debugger
	//More info here: https://www.hex-rays.com/products/ida/support/sdkdoc/dbg_8hpp.html#ac67a564945a2c1721691aa2f657a908c
	invalidate_dbgmem_contents(addr, size);
	if (get_many_bytes(addr, &buffer, size))
		msg("reading success\n");
	return triton::utils::fromBufferToUint<triton::uint128>(buffer);
}