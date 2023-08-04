#pragma once

/*
	DataSequence.h

	data-sequence class

	Copyright J Brown 1999-2006
	www.catch22.net

	Freeware
*/

#include "core\Array.h"

//
//	Define the underlying string/character type of the sequence.
//
//	'seqchar_t' can be redefined to BYTE, WCHAR, ULONG etc 
//	depending on what kind of string you want your sequence to hold
//
#ifdef	UNICODE
	typedef WCHAR				seqchar_t;
#else
	typedef unsigned char		seqchar_t;
#endif

#ifdef	_WIN64
	typedef unsigned __int64	size_w;
#else
	typedef unsigned long		size_w;
#endif

const size_w MAX_SEQUENCE_LENGTH = ((size_w)(-1) / sizeof(seqchar_t));

//
//	sequence class!
//
class CDataSequence
{
public:
	// forward declare the nested helper-classes
	class			span;
	class			span_range;
	class			buffer_control;
	class			ref;
	enum			action;

public:
	// sequence construction
	CDataSequence ();
	~CDataSequence ();

	// Initialization
	HRESULT		Initialize (VOID);

	//
	// prepare with a file
	//
	HRESULT		prepare ();
	VOID		clear ();

	//
	// prepare from an in-memory buffer
	//
	HRESULT		prepare (const seqchar_t* buffer, size_t length);
	bool		is_prepared () const;

	//
	//	sequence statistics
	//
	size_w		size () const;
	
	//
	// sequence manipulation 
	//
	HRESULT		insert (size_w index, const seqchar_t *buf, size_w length);
	HRESULT		insert (size_w index, const seqchar_t  val, size_w count);
	HRESULT		insert (size_w index, const seqchar_t  val);
	HRESULT		replace(size_w index, const seqchar_t *buf, size_w length, size_w erase_length);
	HRESULT		replace(size_w index, const seqchar_t *buf, size_w length);
	HRESULT		replace(size_w index, const seqchar_t  val, size_w count);
	HRESULT		replace(size_w index, const seqchar_t  val);
	HRESULT		erase  (size_w index, size_w len);
	HRESULT		erase  (size_w index);
	HRESULT		append (const seqchar_t *buf, size_w len);
	HRESULT		append (const seqchar_t val);
	void		breakopt();

	//
	// undo/redo support
	//
	HRESULT		undo();
	HRESULT		redo();
	bool		canundo() const;
	bool		canredo() const;
	void		group();
	void		ungroup();
	size_w		event_index() const  { return undoredo_index; }
	size_w		event_length() const { return undoredo_length; }
	void		event_pair (__out sysint* pnUndo, __out sysint* pnRedo);

#ifdef DEBUG_SEQUENCE
	// print out the sequence
	void		debug1();
	void		debug2();
#endif

	//
	// access and iteration
	//
	HRESULT		render(size_w index, seqchar_t *buf, size_w len, __out size_w* pnCopied) const;
	HRESULT		render_length(size_w index, __out size_w* pnSize) const;
	HRESULT		render_offsets (TArray<size_w>& aOffsets, seqchar_t seqBreak) const;
	size_w		longest_line (seqchar_t seqBreak, seqchar_t seqTab, int nTabWidth);
	seqchar_t	peek(size_w index) const;
	HRESULT		poke(size_w index, seqchar_t val);

	HRESULT		StreamOut (ISequentialStream* pstmData) const;

	seqchar_t	operator[] (size_w index) const;
	ref			operator[] (size_w index);

private:
	typedef			TArray<span_range*>	eventstack;
	typedef			TArray<buffer_control*> bufferlist;

	//
	//	Span-table management
	//
	void			deletefromsequence(span **sptr);
	span		*	spanfromindex(size_w index, size_w *spanindex) const;
	void			scan(span *sptr);
	size_w			sequence_length;
	span		*	head;
	span		*	tail;	
	span		*	frag1;
	span		*	frag2;
	
	//
	//	Undo and redo stacks
	//
	HRESULT			initundo(size_w index, size_w length, action act, __deref_out span_range** ppRange);
	void			restore_spanrange(span_range *range, bool undo_or_redo);
	void			swap_spanrange(span_range *src, span_range *dest);
	HRESULT			undoredo(eventstack &source, eventstack &dest);
	void			clearstack(eventstack &source);
	span_range *	stackback(eventstack &source, size_t idx);

	eventstack		undostack;
	eventstack		redostack;
	size_t			group_id;
	size_t			group_refcount;
	size_w			undoredo_index;
	size_w			undoredo_length;

	//
	//	File and memory buffer management
	//
	HRESULT			alloc_buffer(size_t size, __deref_out buffer_control** ppBuffer);
	HRESULT			alloc_modifybuffer(size_t size, __deref_out buffer_control** ppBuffer);
	HRESULT			import_buffer(const seqchar_t *buf, size_t len, size_t *buffer_offset);

	bufferlist		buffer_list;
	int				modifybuffer_id;
	int				modifybuffer_pos;

	//
	//	Sequence manipulation
	//
	HRESULT			insert_worker (size_w index, const seqchar_t *buf, size_w len, action act);
	HRESULT			erase_worker  (size_w index, size_w len, action act);
	bool			can_optimize  (action act, size_w index);
	void			record_action (action act, size_w index);

	size_w			lastaction_index;
	action			lastaction;
	bool			can_quicksave;
};

//
//	CDataSequence::action
//
//	enumeration of the type of 'edit actions' our sequence supports.
//	only important when we try to 'optimize' repeated operations on the
//	sequence by coallescing them into a single span.
//
enum CDataSequence::action
{ 
	action_invalid, 
	action_insert, 
	action_erase, 
	action_replace 
};

//
//	CDataSequence::span
//
//	private class to the sequence
//
class CDataSequence::span
{
	friend class CDataSequence;
	friend class span_range;
	
public:
	// constructor
	span(size_w off, size_w len, int buf, span *nx = 0, span *pr = 0) 
			:
			next(nx), 
			prev(pr),
			offset(off), 
			length(len), 
			buffer(buf)
	  {
		  static int count = -2;
		  id = count++;
	  }

private:

	span   *next;
	span   *prev;	// double-link-list 
	
	size_w  offset;
	size_w  length;
	int     buffer;

	int		id;
};

//
//	CDataSequence::span_range
//
//	private class to the sequence. Used to represent a contiguous range of spans.
//	used by the undo/redo stacks to store state. A span-range effectively represents
//	the range of spans affected by an event (operation) on the sequence
//  
//
class CDataSequence::span_range
{
	friend class CDataSequence;

public:

	// constructor
	span_range(	size_w	seqlen = 0, 
				size_w	idx    = 0, 
				size_w	len    = 0, 
				action	a      = action_invalid,
				bool	qs     = false, 
				size_t	id     = 0
			) 
		: 
		first(0), 
		last(0), 
		boundary(true), 
		sequence_length(seqlen), 	
		index(idx),
		length(len),
		act(a),
		quicksave(qs),
		group_id(id)
	{
	}

	// destructor does nothing - because sometimes we don't want
	// to free the contents when the span_range is deleted. e.g. when
	// the span_range is just a temporary helper object. The contents
	// must be deleted manually with span_range::free
	~span_range()
	{
	}

	// separate 'destruction' used when appropriate
	void free()
	{
		span *sptr, *next, *term;
		
		if(boundary == false)
		{
			// delete the range of spans
			for(sptr = first, term = last->next; sptr && sptr != term; sptr = next)
			{
				next = sptr->next;
				__delete sptr;
			}
		}
	}

	// add a span into the range
	HRESULT link(span *sptr)
	{
		// link() might be called with an unchecked "new" allocation.
		if(NULL == sptr)
			return E_OUTOFMEMORY;

		if(first == NULL)
		{
			// first time a span has been added?
			first = sptr;
		}
		else
		{
			// otherwise chain the spans together.
			last->next = sptr;
			sptr->prev = last;
		}

		last     = sptr;
		boundary = false;

		return S_OK;
	}

	// join two span-ranges together
	void append(span_range *range)
	{
		if(range->boundary == false)
		{	
			if(boundary)
			{
				first       = range->first;
				last        = range->last;
				boundary    = false;
			}
			else
			{
				range->first->prev = last;
				last->next  = range->first;
				last		= range->last;
			}
		}
	}

	// join two span-ranges together. used only for 'back-delete'
	void prepend(span_range *range)
	{
		if(range->boundary == false)
		{
			if(boundary)
			{
				first       = range->first;
				last        = range->last;
				boundary    = false;
			}
			else
			{
				range->last->next = first;
				first->prev	= range->last;
				first		= range->first;
			}
		}
	}
	
	// An 'empty' range is represented by storing pointers to the
	// spans ***either side*** of the span-boundary position. Input is
	// always the span following the boundary.
	void spanboundary(span *before, span *after)
	{
		first    = before;
		last     = after;
		boundary = true;
	}
	
private:
	
	// the span range
	span	*first;
	span	*last;
	bool	 boundary;

	// sequence state
	size_w	 sequence_length;
	size_w	 index;
	size_w	 length;
	action	 act;
	bool	 quicksave;
	size_t	 group_id;
};

//
//	CDataSequence::ref
//
//	temporary 'reference' to the sequence, used for
//  non-const array access with CDataSequence::operator[]
//
class CDataSequence::ref
{
public:
	ref(CDataSequence* s, size_w i) 
		:  
		seq(s),  
		index(i) 
	{
	}

	operator seqchar_t() const		
	{ 
		return seq->peek(index);	          
	}
	
	ref& operator= (seqchar_t rhs)	
	{ 
		seq->poke(index, rhs); 
		return *this;	
	}

private:
	size_w index;
	CDataSequence* seq;
};

//
//	buffer_control
//
class CDataSequence::buffer_control
{
public:
	seqchar_t* buffer;
	size_w	 length;
	size_w	 maxsize;
	int		 id;
};
