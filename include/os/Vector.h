#ifndef __OS_VECTOR_H
#define __OS_VECTOR_H

#include <stdlib.h>

namespace OS
{

template<class T> class Vector
{
protected:
	T *m_ptr;
	unsigned m_size;
	unsigned m_allocated;

public:
	Vector()
	{
		m_ptr = NULL;
		m_size = 0;
		m_allocated = 0;
	}

	~Vector()
	{
		free(m_ptr);
	}

	T *Expand(unsigned n)
	{
		if (m_size + n > m_allocated)
		{
			if (m_allocated == 0)
				m_allocated = 1;

			while (m_size + n > m_allocated)
				m_allocated *= 2;

			m_ptr = (T*) realloc(m_ptr, m_allocated * sizeof(T));
			if (m_ptr == NULL)
			{
				m_size = 0;
				m_allocated = 0;
				return NULL;
			}
		}

		m_size += n;
		return m_ptr + m_size - n;
	}

	void Add(T t)
	{
		*Expand(1) = t;
	}

	unsigned Size() const
	{
		return m_size;
	}

	T& operator[](unsigned i)
	{
		return m_ptr[i];
	}
};

};

#endif
