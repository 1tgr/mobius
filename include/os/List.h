#ifndef __OS_LIST_H
#define __OS_LIST_H

namespace OS
{

template<class T> class List
{
protected:
	template<class U> struct Entry
	{
		Entry<U> *prev, *next;
		U item;

		Entry(const T& _item) : item(_item)
		{
		}
	};

	template<class U> class ListIterator
	{
	protected:
		Entry<U> *m_entry;

	public:
		ListIterator(Entry<U> *entry)
		{
			m_entry = entry;
		}

		ListIterator()
		{
			m_entry = NULL;
		}

		U& operator*()
		{
			return m_entry->item;
		}

		U* operator->()
		{
			return &m_entry->item;
		}

		ListIterator<U>& operator++()
		{
			m_entry = m_entry->next;
			return *this;
		}

		ListIterator<U>& operator--()
		{
			m_entry = m_entry->prev;
			return *this;
		}

		bool operator==(const ListIterator<U>& other) const
		{
			return m_entry == other.m_entry;
		}

		bool operator!=(const ListIterator<U>& other) const
		{
			return m_entry != other.m_entry;
		}
	};

	Entry<T> *m_first, *m_last;
	unsigned m_size;

public:
	typedef ListIterator<T> Iterator;
	typedef const ListIterator<T> ConstIterator;

	List()
	{
		m_first = m_last = NULL;
		m_size = 0;
	}

	Iterator Begin()
	{
		return m_first;
	}

	Iterator End()
	{
		return NULL;
	}

	Iterator Add(T item)
	{
		Entry<T> *e = new Entry<T>(item);
		e->prev = m_last;
		e->next = NULL;
		if (m_last != NULL)
			m_last->next = e;
		m_last = e;
		if (m_first == NULL)
			m_first = e;
		m_size++;
		return e;
	}

	unsigned Size() const
	{
		return m_size;
	}
};

};

#endif
