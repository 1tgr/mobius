#ifndef __KERNEL_OBJ_H
#define __KERNEL_OBJ_H

#ifdef __cplusplus
extern "C"
{
#endif

struct process_t;

/*!
 *	\defgroup	obj	Object Services
 *	\ingroup	kernel
 *	@{
 */

struct marshal_entry_t
{
	void* ptr;
};

//! Defines a marshalling handle, able to refer to any marshalled object in a 
//!		specific process.
typedef dword marshal_t;
typedef struct marshal_entry_t marshal_map_t;

marshal_t objMarshal(struct process_t* proc, void* ptr);
void* objUnmarshal(struct process_t* proc, marshal_t mshl);
void objNotifyDelete(struct process_t* proc, marshal_t mshl);

//@}

#ifdef __cplusplus
}
#endif

#endif