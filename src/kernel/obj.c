#include <kernel/kernel.h>
#include <kernel/obj.h>
#include <kernel/proc.h>
#include <kernel/handle.h>
#include <malloc.h>
#include <errno.h>
#include <string.h>

/*!
 *	\ingroup	obj
 *	@{
 */

//! Creates a marshalling handle for the specified object.
/*!
 *	\param	proc	The process which will use the marshalled object
 *	\param	ptr		The interface to marshal
 *	\return	A marshalling handle to the interface provided, or NULL on
 *		error.
 */
marshal_t objMarshal(process_t* proc, void* ptr)
{
	int i;

	if (proc->marshal_map == NULL)
	{
		proc->marshal_map = (marshal_map_t*) hndAlloc(sizeof(marshal_t) * 256, proc);
		if (proc->marshal_map == NULL)
			return 0;
		memset(proc->marshal_map, NULL, sizeof(marshal_t) * 256);
	}

	for (i = 0; i < 255; i++)
		if (proc->marshal_map[i].ptr == ptr)
			return i;

	i = proc->last_marshal + 1;
	do
	{
		if (i > 255)
		{
			errno = EMFILE;
			return NULL;
		}

		i++;
	} while (proc->marshal_map[i].ptr != NULL);

	proc->last_marshal = i;
	proc->marshal_map[i].ptr = ptr;
	return i;
}

//! Retrieves the object referred to by the specified marshalling handle.
/*!
 *	\param	proc	The process which will uses the marshalled object
 *	\param	mshl	The marshalling handle obtained from objMarshal()
 *	\return	The interface referred to by the specified marshalling handle,
 *		or NULL on error.
 */
void* objUnmarshal(process_t* proc, marshal_t mshl)
{
	if (proc->marshal_map && proc->marshal_map[mshl].ptr)
		return proc->marshal_map[mshl].ptr;
	else
	{
		errno = ENOTFOUND;
		return NULL;
	}
}

//! Notifies the kernel that the specified object has been deleted.
/*!
 *	Marshal handles to objects become invalid once the object they refer to
 *		has been deleted. The kernel must be notified of any object deletions.
 *	\param	proc	The process which used the marshalled object
 *	\param	mshl	The marshalling handle obtained from objMarshal()
 */
void objNotifyDelete(process_t* proc, marshal_t mshl)
{
	if (proc->marshal_map)
		proc->marshal_map[mshl].ptr = NULL;
}

//@}
