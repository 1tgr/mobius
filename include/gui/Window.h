// $Id$
#ifndef __GUI_WINDOW_H
#define __GUI_WINDOW_H

#include "winmgr.h"
#include "types.h"

/**
 *	\addtogroup	gui	GUI
 *	@{
 */

namespace OS
{
#include "ipc.h"
}

#include <wchar.h>

namespace OS
{

/*!
 *	\brief	Wraps a window handle
 */
class Window
{
public:
	//! Handle to the window
	wnd_h m_handle;

	Window(wnd_h handle = 0)
	{
		m_handle = handle;
	}

	operator wnd_h()
	{
		return m_handle;
	}

	//! Closes the window
	void Close()
	{
		WndClose(m_handle);
		m_handle = 0;
	}

	/*!
	 *	\brief	Notifies the window manager that the application is about to start 
	 *		repainting this window.
	 *
	 *	Called automatically by \b Painter's constructor. It returns a graphics 
	 *		handle for drawing, and the window's current invalid rectangle.
	 *		The invalid rectangle covers the area which needs repainting.
	 *
	 *	\param	rect	Pointer to an \b rect_t structure which will receive the
	 *		window's invalid rectangle, or \b NULL if this is not needed.
	 *	\return	A graphics handle
	 */
	gfx_h BeginPaint(rect_t *rect)
	{
		return WndBeginPaint(m_handle, rect);
	}

	/*!
	 *	\brief	Invalidates an area of the window
	 *
	 *	The rectangle provided is added to the window's invalid rectangle and a 
	 *		paint message is generated.
	 *
	 *	\param	rect	Area to invalidate
	 */
	void Invalidate(const rect_t& rect)
	{
		WndInvalidate(m_handle, &rect);
	}

	/*!
	 *	\brief	Retrieves one of the window's attributes
	 *
	 *	The window manager stores a list of attributes for each window, which 
	 *		can be retrieved from any process.
	 *
	 *	\param	code	Identifier of the attribute to retrieve. System-
	 *		defined attributes are:
	 *			- \b WND_ATTR_POSITION: the window's position.
	 *			- \b WND_ATTR_CLIENT_DATA: an opaque 32-bit value
	 *	\param	data	Pointer to a \b wnd_data_t structure which will 
	 *		receive the attribute's data
	 *	\return	\b true if the attribute existed, \b false otherwise
	 */
	bool GetAttribute(uint32_t code, wnd_data_t *data)
	{
		return WndGetAttribute(m_handle, code, data);
	}

	void SetAttribute(uint32_t code, const wnd_data_t *data)
	{
		WndSetAttribute(m_handle, code, data);
	}

	bool HasFocus()
	{
		return WndHasFocus(m_handle);
	}

	void SetFocus()
	{
		WndSetFocus(m_handle);
	}

	wnd_h WindowAtPoint(int x, int y)
	{
		return WndWindowAtPoint(m_handle, x, y);
	}

	void SetCapture(bool set)
	{
		WndSetCapture(m_handle, set);
	}

	void BringToFront()
	{
	}
};


//! Abstract base class for an object that can handle messages
class GUI_EXPORT MessageHandler
{
protected:
	virtual void DoHandleMessage(const wnd_message_t *msg) = 0;

public:
	static void HandleMessage(const wnd_message_t *msg);
};


/*!
 *	\brief	Base class for custom window classes
 *
 *	This class will create a window and process messages for it
 */
class GUI_EXPORT WindowImpl : public Window, public MessageHandler
{
protected:
	wchar_t *m_title;
	Rect m_position;
	WindowImpl *m_parent;
	const wnd_message_t *m_message;

	wnd_h DoCreateWindow(WindowImpl *parent, const wchar_t *title, const rect_t& position);
	void DoHandleMessage(const wnd_message_t *msg);

	/*!
	 *	\brief Handler for \b MSG_QUIT messages
	 */
	virtual void OnQuit()
	{
	}

	/*!
	 *	\brief Handler for \b MSG_PAINT messages
	 *
	 *	You should declare a \b Painter object in your \b OnPaint handler, or 
	 *		call \b BeginPaint.
	 */
	virtual void OnPaint()
	{
	}

	/*!
	 *	\brief	Handler for \b MSG_KEYDOWN messages
	 *
	 *	\param	key	Key code; see \b <os/keyboard.h>
	 */
	virtual void OnKeyDown(uint32_t key)
	{
		if (m_parent != 0)
			m_parent->DoHandleMessage(m_message);
	}

	/*!
	 *	\brief	Handler for \b MSG_KEYUP messages
	 *
	 *	\param	key	Key code; see \b <os/keyboard.h>
	 */
	virtual void OnKeyUp(uint32_t key)
	{
		if (m_parent != 0)
			m_parent->DoHandleMessage(m_message);
	}

	/*!
	 *	\brief	Handler for \b MSG_MOUSEMOVE messages
	 *
	 *	\param	pt	Position of the mouse pointer after it was moved
	 */
	virtual void OnMouseMove(point_t pt)
	{
	}

	/*!
	 *	\brief	Handler for \b MSG_MOUSEDOWN messages
	 *
	 *	\param	\pt	Position of the mouse pointer when the button was pressed
	 */
	virtual void OnMouseDown(point_t pt)
	{
	}

	/*!
	 *	\brief	Handler for \b MSG_MOUSEUP messages
	 *
	 *	\param	\pt	Position of the mouse pointer when the button was released
	 */
	virtual void OnMouseUp(point_t pt)
	{
	}

	virtual void OnClose(wnd_h handle)
	{
	}

	/*!
	 *	\brief	Handler for \b MSG_FOCUS messages
	 */
	virtual void OnFocus()
	{
	}

	/*!
	 *	\brief	Handler for \b MSG_FOCUS messages
	 */
	virtual void OnBlur()
	{
	}

	/*!
	 *	\brief	Called when a child window is created
	 */
	virtual void OnAddChild(WindowImpl *child)
	{
	}

	/*!
	 *	\brief	Called when a child window is closed
	 */
	virtual void OnRemoveChild(WindowImpl *child)
	{
	}

	/*!
	 *	\brief	Called after the window is created
	 */
	virtual bool OnCreate()
	{
		return true;
	}

	/*!
	 *	\brief	Called when the window is resized
	 */
	virtual void OnSize(int width, int height)
	{
	}

public:
	WindowImpl();
	virtual ~WindowImpl();

	/*!
	 *	\brief	Creates the window
	 *
	 *	\param	parent	Pointer to the parent window, or \b NULL to create a 
	 *		top-level window (frame).
	 *	\param	title	Title of the window or control, or \b NULL for none.
	 *	\param	position	Position of the window
	 */
	bool Create(WindowImpl *parent, 
		const wchar_t *title, 
		const rect_t& position = Rect(0, 0, 0, 0));
	void AddChild(WindowImpl *child);
	void RemoveChild(WindowImpl *child);
	WindowImpl *GetParentFrame();
	void SetPosition(const rect_t& pos);
};

};

//! @}

#endif
