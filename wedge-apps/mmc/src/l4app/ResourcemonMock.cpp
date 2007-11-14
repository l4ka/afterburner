/*
 * dummy implementation of the resourcemon access functions
 */
#include "l4app/Resourcemon.h"
 
namespace l4app
{

/**
 * @brief get a VMInfo from the RM
 */
int
Resourcemon::getVMInfo(L4_Word_t spaceId, VMInfo& vmInfo) const
{
	return 0;
}

/**
 * @brief map VM space into this address space
 */
int
Resourcemon::getVMSpace(L4_Word_t spaceId, char*& vm_space) const
{
	return 0;
}

/**
 * @brief delete the local successfully migrated VM
 */
int
Resourcemon::deleteVM(L4_Word_t spaceId) const
{
	return 0;
}

}; // namespace l4app
