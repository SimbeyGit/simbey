/*
	DataSequence.cpp

	data-sequence class

	Copyright J Brown 1999-2006
	www.catch22.net

	Freeware
*/

#ifdef DEBUG_SEQUENCE
	#include <stdarg.h>
	#include <stdio.h>
#endif

#include <windows.h>
#include "core\CoreDefs.h"
#include "DataSequence.h"

CDataSequence::CDataSequence ()
{
	head = tail		= 0;
	sequence_length = 0;
	group_id		= 0;
	group_refcount	= 0;
}

CDataSequence::~CDataSequence ()
{
	clear();

	__delete head;
	__delete tail;
}

HRESULT CDataSequence::Initialize (VOID)
{
	HRESULT hr = S_OK;

	record_action(action_invalid, 0);

	head			= __new span(0, 0, 0);
	CheckAlloc(head);

	tail			= __new span(0, 0, 0);
	CheckAlloc(tail);

	head->next		= tail;
	tail->prev		= head;

Cleanup:
	return hr;
}

HRESULT CDataSequence::prepare ()
{
	HRESULT hr;
	CDataSequence::buffer_control* pBuffer;

	sequence_length = 0;

	Check(alloc_modifybuffer(0x10000, &pBuffer));

	record_action(action_invalid, 0);
	group_id		= 0;
	group_refcount	= 0;
	undoredo_index	= 0;
	undoredo_length = 0;

Cleanup:
	return hr;
}

HRESULT CDataSequence::prepare (const seqchar_t* buffer, size_t length)
{
	HRESULT hr;
	buffer_control* bc;
	span* sptr = NULL;

	clear();

	Check(prepare());

	Check(alloc_modifybuffer(length, &bc));
	memcpy(bc->buffer, buffer, length * sizeof(seqchar_t));
	bc->length = length;

	sptr = __new span(0, length, bc->id, tail, head);
	CheckAlloc(sptr);

	head->next = sptr;
	tail->prev = sptr;

	sequence_length = length;

Cleanup:
	return hr;
}

bool CDataSequence::is_prepared () const
{
	return 0 != buffer_list.Length();
}

void CDataSequence::clearstack (eventstack &dest)
{
	for(sysint i = 0; i < dest.Length(); i++)
	{
		dest[i]->free();
		__delete dest[i];
	}

	dest.Clear();
}

#ifdef DEBUG_SEQUENCE

void CDataSequence::debug1 ()
{
	span *sptr;

	for(sptr = head; sptr; sptr = sptr->next)
	{
		char *buffer = (char *)buffer_list[sptr->buffer]->buffer;
		printf("%.*s", sptr->length, buffer + sptr->offset);
	}

	printf("\n");
}

void CDataSequence::debug2 ()
{
	span *sptr;

	printf("**********************\n");
	for(sptr = head; sptr; sptr = sptr->next)
	{
		char *buffer = (char *)buffer_list[sptr->buffer]->buffer;
		
		printf("[%d] [%4d %4d] %.*s\n", sptr->id, 
			sptr->offset, sptr->length,
			sptr->length, buffer + sptr->offset);
	}

	printf("-------------------------\n");

	for(sptr = tail; sptr; sptr = sptr->prev)
	{
		char *buffer = (char *)buffer_list[sptr->buffer]->buffer;
		
		printf("[%d] [%4d %4d] %.*s\n", sptr->id, 
			sptr->offset, sptr->length,
			sptr->length, buffer + sptr->offset);
	}

	printf("**********************\n");

	for(sptr = head; sptr; sptr = sptr->next)
	{
		char *buffer = (char *)buffer_list[sptr->buffer]->buffer;
		printf("%.*s", sptr->length, buffer + sptr->offset);
	}

	printf("\nsequence length = %d chars\n", sequence_length);
	printf("\n\n");
}

#endif

//
//	Allocate a buffer and add it to our 'buffer control' list
//
HRESULT CDataSequence::alloc_buffer (size_t maxsize, __deref_out CDataSequence::buffer_control** ppBuffer)
{
	HRESULT hr;
	buffer_control* bc = __new buffer_control;

	CheckAlloc(bc);

	// allocate a new buffer of byte/wchar/long/whatever
	bc->buffer = __new seqchar_t[maxsize];
	CheckAlloc(bc->buffer);

	bc->length  = 0;
	bc->maxsize = maxsize;
	bc->id		= buffer_list.Length();		// assign the id

	Check(buffer_list.Append(bc));
	*ppBuffer = bc;
	bc = NULL;

Cleanup:
	__delete bc;
	return hr;
}

HRESULT CDataSequence::alloc_modifybuffer (size_t maxsize, __deref_out CDataSequence::buffer_control** ppBuffer)
{
	HRESULT hr;
	buffer_control* bc;
	
	Check(alloc_buffer(maxsize, &bc));

	modifybuffer_id  = bc->id;
	modifybuffer_pos = 0;

	*ppBuffer = bc;

Cleanup:
	return hr;
}

//
//	Import the specified range of data into the sequence so we have our own private copy
//
HRESULT CDataSequence::import_buffer (const seqchar_t *buf, size_t len, size_t *buffer_offset)
{
	HRESULT hr;
	buffer_control* bc;

	// get the current modify-buffer
	bc = buffer_list[modifybuffer_id];

	// if there isn't room then allocate a new modify-buffer
	if(bc->length + len >= bc->maxsize)
	{
		Check(alloc_modifybuffer(len + 0x10000, &bc));
		
		// make sure that no old spans use this buffer
		record_action(action_invalid, 0);
	}

	// import the data
	memcpy(bc->buffer + bc->length, buf, len * sizeof(seqchar_t));
	
	*buffer_offset = bc->length;
	bc->length += len;
	hr = S_OK;

Cleanup:
	return hr;
}

//
//	CDataSequence::spanfromindex
//
//	search the spanlist for the span which encompasses the specified index position
//
//	index		- character-position index
//	*spanindex  - index of span within sequence
//
CDataSequence::span* CDataSequence::spanfromindex (size_w index, size_w *spanindex = 0) const
{
	span* sptr;
	size_w curidx = 0;

	// scan the list looking for the span which holds the specified index
	for(sptr = head->next; sptr->next; sptr = sptr->next)
	{
		if(index >= curidx && index < curidx + sptr->length)
		{
			if(spanindex) 
				*spanindex = curidx;

			return sptr;
		}

		curidx += sptr->length;
	}

	// insert at tail
	if(sptr && index == curidx)
	{
		*spanindex = curidx;
		return sptr;
	}

	return NULL;
}

void CDataSequence::swap_spanrange(span_range *src, span_range *dest)
{
	if(src->boundary)
	{
		if(!dest->boundary)
		{
			src->first->next = dest->first;
			src->last->prev  = dest->last;
			dest->first->prev = src->first;
			dest->last->next  = src->last;
		}
	}
	else
	{
		if(dest->boundary)
		{
			src->first->prev->next = src->last->next;
			src->last->next->prev  = src->first->prev;
		}
		else
		{
			src->first->prev->next = dest->first;
			src->last->next->prev  = dest->last;
			dest->first->prev = src->first->prev;
			dest->last->next = src->last->next;
		}	
	}
}

void CDataSequence::restore_spanrange (span_range *range, bool undo_or_redo)
{
	if(range->boundary)
	{
		span *first = range->first->next;
		span *last  = range->last->prev;

		// unlink spans from main list
		range->first->next = range->last;
		range->last->prev  = range->first;

		// store the span range we just removed
		range->first = first;
		range->last  = last;
		range->boundary = false;
	}
	else
	{
		span *first = range->first->prev;
		span *last  = range->last->next;

		// are we moving spans into an "empty" region?
		// (i.e. inbetween two adjacent spans)
		if(first->next == last)
		{
			// move the old spans back into the empty region
			first->next = range->first;
			last->prev  = range->last;

			// store the span range we just removed
			range->first  = first;
			range->last   = last;
			range->boundary  = true;
		}
		// we are replacing a range of spans in the list,
		// so swap the spans in the list with the one's in our "undo" event
		else
		{
			// find the span range that is currently in the list
			first = first->next;
			last  = last->prev;

			// unlink the the spans from the main list
			first->prev->next = range->first;
			last->next->prev  = range->last;

			// store the span range we just removed
			range->first = first;
			range->last  = last;
			range->boundary = false;
		}
	}

	// update the 'sequence length' and 'quicksave' states
	SwapData(range->sequence_length,    sequence_length);
	SwapData(range->quicksave,			 can_quicksave);

	undoredo_index	= range->index;

	if(range->act == action_erase && undo_or_redo == true || 
		range->act != action_erase && undo_or_redo == false)
	{
		undoredo_length = range->length;
	}
	else
	{
		undoredo_length = 0;
	}
}

//
//	CDataSequence::undoredo
//
//	private routine used to undo/redo spanrange events to/from 
//	the sequence - handles 'grouped' events
//
HRESULT CDataSequence::undoredo (eventstack &source, eventstack &dest)
{
	HRESULT hr;
	span_range* range = NULL;
	size_t group_id;

	CheckIf(source.Length() == 0, HRESULT_FROM_WIN32(ERROR_EMPTY));

	// make sure that no "optimized" actions can occur
	record_action(action_invalid, 0);

	group_id = source[source.Length() - 1]->group_id;

	do
	{
		// remove the next event from the source stack
		source.Remove(source.Length() - 1, &range);

		// add event onto the destination stack
		Check(dest.Append(range));

		// do the actual work
		restore_spanrange(range, &source == &undostack);
		range = NULL;

	} while(0 != source.Length() && (source[source.Length() - 1]->group_id == group_id && group_id != 0));

Cleanup:
	SafeDelete(range);
	return hr;
}

// 
//	UNDO the last action
//
HRESULT CDataSequence::undo ()
{
	return undoredo(undostack, redostack);
}

//
//	REDO the last UNDO
//
HRESULT CDataSequence::redo ()
{
	return undoredo(redostack, undostack);
}

//
//	Will calling CDataSequence::undo change the sequence?
//
bool CDataSequence::canundo () const
{
	return undostack.Length() != 0;
}

//
//	Will calling CDataSequence::redo change the sequence?
//
bool CDataSequence::canredo () const
{
	return redostack.Length() != 0;
}

//
//	Group repeated actions on the sequence (insert/erase etc)
//	into a single 'undoable' action
//
void CDataSequence::group()
{
	if(group_refcount == 0)
	{
		if(++group_id == 0)
			++group_id;

		group_refcount++;
	}
}

//
//	Close the grouping
//
void CDataSequence::ungroup()
{
	if(group_refcount > 0)
		group_refcount--;
}

void CDataSequence::event_pair (__out sysint* pnUndo, __out sysint* pnRedo)
{
	*pnUndo = undostack.Length();
	*pnRedo = redostack.Length();
}

//
//	Return logical length of the sequence
//
size_w CDataSequence::size () const
{
	return sequence_length;
}

//
//	CDataSequence::initundo
//
//	create a new (empty) span range and save the current sequence state
//
HRESULT CDataSequence::initundo (size_w index, size_w length, action act, __deref_out CDataSequence::span_range** ppRange)
{
	HRESULT hr;
	span_range* pRange = __new span_range(
								sequence_length, 
								index,
								length,
								act,
								can_quicksave, 
								group_refcount ? group_id : 0
								);
	CheckAlloc(pRange);
	Check(undostack.Append(pRange));
	*ppRange = pRange;
	pRange = NULL;

Cleanup:
	SafeDelete(pRange);
	return hr;
}

CDataSequence::span_range* CDataSequence::stackback(eventstack &source, size_t idx)
{
	sysint length = source.Length();
	
	if(length > 0 && idx < (size_t)length)
	{
		return source[length - idx - 1];
	}

	return NULL;
}

void CDataSequence::record_action (action act, size_w index)
{
	lastaction_index = index;
	lastaction       = act;
}

bool CDataSequence::can_optimize (action act, size_w index)
{
	return (lastaction == act && lastaction_index == index);
}

//
//	CDataSequence::insert_worker
//
HRESULT CDataSequence::insert_worker (size_w index, const seqchar_t *buf, size_w length, action act)
{
	HRESULT		hr;
	span *		sptr;
	size_w		spanindex;
	size_t		modbuf_offset;
	span_range	newspans;
	size_w		insoffset;

	CheckIf(index > sequence_length, HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));

	// find the span that the insertion starts at
	sptr = spanfromindex(index, &spanindex);
	CheckIf(NULL == sptr, E_FAIL);

	// ensure there is room in the modify buffer...
	// allocate a new buffer if necessary and then invalidate span cache
	// to prevent a span using two buffers of data
	Check(import_buffer(buf, length, &modbuf_offset));

	clearstack(redostack);
	insoffset = index - spanindex;

	// special-case #1: inserting at the end of a prior insertion, at a span-boundary
	if(insoffset == 0 && can_optimize(act, index))
	{
		// simply extend the last span's length
		span_range *event = undostack[undostack.Length() - 1];
		sptr->prev->length	+= length;
		event->length		+= length;
	}
	// general-case #1: inserting at a span boundary?
	else if(insoffset == 0)
	{
		span_range* oldspans;

		//
		// Create a new undo event; because we are inserting at a span
		// boundary there are no spans to replace, so use a "span boundary"
		//
		Check(initundo(index, length, act, &oldspans));
		oldspans->spanboundary(sptr->prev, sptr);
		
		// allocate new span in the modify buffer
		Check(newspans.link(__new span(
			modbuf_offset, 
			length, 
			modifybuffer_id)
			));
		
		// link the span into the sequence
		swap_spanrange(oldspans, &newspans);
	}
	// general-case #2: inserting in the middle of a span
	else
	{
		span_range* oldspans;

		//
		//	Create a new undo event and add the span
		//  that we will be "splitting" in half
		//
		Check(initundo(index, length, act, &oldspans));
		Check(oldspans->link(sptr));

		//	span for the existing data before the insertion
		Check(newspans.link(__new span(
							sptr->offset, 
							insoffset, 
							sptr->buffer)
						));

		// make a span for the inserted data
		Check(newspans.link(__new span(
							modbuf_offset, 
							length, 
							modifybuffer_id)
						));

		// span for the existing data after the insertion
		Check(newspans.link(__new span(
							sptr->offset + insoffset, 
							sptr->length - insoffset, 
							sptr->buffer)
						));

		swap_spanrange(oldspans, &newspans);
	}

	sequence_length += length;

Cleanup:
	return hr;
}

//
//	CDataSequence::insert
//
//	Insert a buffer into the sequence at the specified position.
//	Consecutive insertions are optimized into a single event
//
HRESULT CDataSequence::insert (size_w index, const seqchar_t *buf, size_w length)
{
	HRESULT hr = insert_worker(index, buf, length, action_insert);
	if(SUCCEEDED(hr))
		record_action(action_insert, index + length);
	return hr;
}

//
//	CDataSequence::insert
//
//	Insert specified character-value into sequence
//
HRESULT CDataSequence::insert (size_w index, const seqchar_t val)
{
	return insert(index, &val, 1);
}

//
//	CDataSequence::deletefromsequence
//
//	Remove + delete the specified *span* from the sequence
//
void CDataSequence::deletefromsequence(span **psptr)
{
	span *sptr = *psptr;
	sptr->prev->next = sptr->next;
	sptr->next->prev = sptr->prev;

	memset(sptr, 0, sizeof(span));
	__delete sptr;
	*psptr = 0;
}

//
//	CDataSequence::erase_worker
//
HRESULT CDataSequence::erase_worker (size_w index, size_w length, action act)
{
	HRESULT		hr;
	span		*sptr;
	span_range	 oldspans;
	span_range	 newspans;
	span_range	*event;
	size_w		 spanindex;
	size_w		 remoffset;
	size_w		 removelen;
	bool		 append_spanrange;	

	// make sure we stay within the range of the sequence
	CheckIf(length == 0 || length > sequence_length || index > sequence_length - length, HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));

	// find the span that the deletion starts at
	sptr = spanfromindex(index, &spanindex);
	CheckIf(NULL == sptr, E_FAIL);

	// work out the offset relative to the start of the *span*
	remoffset = index - spanindex;
	removelen = length;

	//
	//	can we optimize?
	//
	//	special-case 1: 'forward-delete'
	//	erase+replace operations will pass through here
	//
	if(index == spanindex && can_optimize(act, index))
	{
		event = stackback(undostack, act == action_replace ? 1 : 0);
		event->length	+= length;
		append_spanrange = true;

		if(frag2 != 0)
		{
			if(length < frag2->length)
			{
				frag2->length	-= length;
				frag2->offset	+= length;
				sequence_length -= length;
				return S_OK;
			}
			else
			{
				if(act == action_replace)
					stackback(undostack, 0)->last = frag2->next;

				removelen	-= sptr->length;
				sptr = sptr->next;
				deletefromsequence(&frag2);
			}
		}
	}
	//
	//	special-case 2: 'backward-delete'
	//	only erase operations can pass through here
	//
	else if(index + length == spanindex + sptr->length && can_optimize(action_erase, index+length))
	{
		event = undostack[undostack.Length() - 1];
		event->length	+= length;
		event->index	-= length;
		append_spanrange = false;

		if(frag1 != 0)
		{
			if(length < frag1->length)
			{
				frag1->length	-= length;
				frag1->offset	+= 0;
				sequence_length -= length;
				return S_OK;
			}
			else
			{
				removelen -= frag1->length;
				deletefromsequence(&frag1);
			}
		}
	}
	else
	{
		append_spanrange = true;
		frag1 = frag2 = 0;

		Check(initundo(index, length, act, &event));
	}

	//
	//	general-case 2+3
	//
	clearstack(redostack);

	// does the deletion *start* mid-way through a span?
	if(remoffset != 0)
	{
		// split the span - keep the first "half"
		Check(newspans.link(__new span(sptr->offset, remoffset, sptr->buffer)));
		frag1 = newspans.first;
		
		// have we split a single span into two?
		// i.e. the deletion is completely within a single span
		if(remoffset + removelen < sptr->length)
		{
			// make a second span for the second half of the split
			Check(newspans.link(__new span(
							sptr->offset + remoffset + removelen, 
							sptr->length - remoffset - removelen, 
							sptr->buffer)
							));

			frag2 = newspans.last;
		}

		removelen -= min(removelen, (sptr->length - remoffset));

		// archive the span we are going to replace
		Check(oldspans.link(sptr));
		sptr = sptr->next;	
	}

	// we are now on a proper span boundary, so remove
	// any further spans that the erase-range encompasses
	while(removelen > 0 && sptr != tail)
	{
		// will the entire span be removed?
		if(removelen < sptr->length)
		{
			// split the span, keeping the last "half"
			Check(newspans.link(__new span(
						sptr->offset + removelen, 
						sptr->length - removelen, 
						sptr->buffer)
						));

			frag2 = newspans.last;
		}

		removelen -= min(removelen, sptr->length);

		// archive the span we are replacing
		Check(oldspans.link(sptr));
		sptr = sptr->next;
	}

	// for replace operations, update the undo-event for the
	// insertion so that it knows about the newly removed spans
	if(act == action_replace && !oldspans.boundary)
		stackback(undostack, 0)->last = oldspans.last->next;

	swap_spanrange(&oldspans, &newspans);
	sequence_length -= length;

	if(append_spanrange)
		event->append(&oldspans);
	else
		event->prepend(&oldspans);
	hr = S_OK;

Cleanup:
	return hr;
}

//
//	CDataSequence::erase 
//
//  "removes" the specified range of data from the sequence. 
//
HRESULT CDataSequence::erase (size_w index, size_w len)
{
	HRESULT hr = erase_worker(index, len, action_erase);
	if(SUCCEEDED(hr))
		record_action(action_erase, index);
	return hr;
}

//
//	CDataSequence::erase
//
//	remove single character from sequence
//
HRESULT CDataSequence::erase (size_w index)
{
	return erase(index, 1);
}

//
//	CDataSequence::replace
//
//	A 'replace' (or 'overwrite') is a combination of erase+inserting
//  (first we erase a section of the sequence, then insert a new block
//  in it's place). 
//
//	Doing this as a distinct operation (erase+insert at the 
//  same time) is really complicated, so I just make use of the existing 
//  CDataSequence::erase and CDataSequence::insert and combine them into action. We
//	need to play with the undo stack to combine them in a 'true' sense.
//
HRESULT CDataSequence::replace(size_w index, const seqchar_t *buf, size_w length, size_w erase_length)
{
	HRESULT hr;
	size_t remlen;

	// make sure operation is within allowed range
	CheckIf(index > sequence_length || MAX_SEQUENCE_LENGTH - index < length, HRESULT_FROM_WIN32(ERROR_INVALID_INDEX));

	// for a "replace" which will overrun the sequence, make sure we 
	// only delete up to the end of the sequence
	remlen = min(sequence_length - index, erase_length);

	// combine the erase+insert actions together
	group();

	// first of all remove the range
	if(remlen > 0 && index < sequence_length)
	{
		hr = erase_worker(index, remlen, action_replace);
		if(FAILED(hr))
		{
			ungroup();
			goto Cleanup;
		}
	}
	
	// then insert the data
	hr = insert_worker(index, buf, length, action_replace);
	ungroup();
	if(SUCCEEDED(hr))
	{
		record_action(action_replace, index + length);
	}
	else
	{
		// failed...cleanup what we have done so far
		record_action(action_invalid, 0);

		span_range* range;
		undostack.Remove(undostack.Length() - 1, &range);
		restore_spanrange(range, true);
		__delete range;
	}

Cleanup:
	return hr;
}

//
//	CDataSequence::replace
//
//	overwrite with the specified buffer
//
HRESULT CDataSequence::replace (size_w index, const seqchar_t *buf, size_w length)
{
	return replace(index, buf, length, length);
}

//
//	CDataSequence::replace
//
//	overwrite with a single character-value
//
HRESULT CDataSequence::replace (size_w index, const seqchar_t val)
{
	return replace(index, &val, 1);
}

//
//	CDataSequence::append
//
//	very simple wrapper around CDataSequence::insert, just inserts at
//  the end of the sequence
//
HRESULT CDataSequence::append (const seqchar_t *buf, size_w length)
{
	return insert(size(), buf, length);
}

//
//	CDataSequence::append
//
//	append a single character to the sequence
//
HRESULT CDataSequence::append (const seqchar_t val)
{
	return append(&val, 1);
}

//
//	CDataSequence::clear
//
//	empty the entire sequence, clear undo/redo history etc
//
VOID CDataSequence::clear ()
{
	span *sptr, *tmp;

	// delete all spans in the sequence
	for(sptr = head->next; sptr != tail; sptr = tmp)
	{
		tmp = sptr->next;
		__delete sptr;
	}

	// re-link the head+tail
	head->next = tail;
	tail->prev = head;

	// delete everything in the undo/redo stacks
	clearstack(undostack);
	clearstack(redostack);

	// delete all memory-buffers
	for(sysint i = 0; i < buffer_list.Length(); i++)
	{
		__delete_array buffer_list[i]->buffer;
		__delete buffer_list[i];
	}

	buffer_list.Clear();
	sequence_length = 0;
}

//
//	CDataSequence::render
//
//	render the specified range of data (index, len) and store in 'dest'
//
//	Returns number of chars copied into destination
//
HRESULT CDataSequence::render(size_w index, seqchar_t* dest, size_w length, __out size_w* pnCopied) const
{
	HRESULT hr = S_OK;
	size_w total = 0, spanoffset;

	// find span to start rendering at
	span* sptr = spanfromindex(index, &spanoffset);
	CheckIf(NULL == sptr, E_FAIL);

	// might need to start mid-way through the first span
	spanoffset = index - spanoffset;

	// copy each span's referenced data in succession
	while(length && sptr != tail)
	{
		size_w cchSpan = min(sptr->length - spanoffset, length);
		seqchar_t* source = buffer_list[sptr->buffer]->buffer + sptr->offset;

		memcpy(dest, source + spanoffset, cchSpan * sizeof(seqchar_t));

		dest	+= cchSpan;
		length	-= cchSpan;
		total	+= cchSpan;

		sptr = sptr->next;
		spanoffset = 0;
	}

	*pnCopied = total;

Cleanup:
	return hr;
}

HRESULT CDataSequence::render_length(size_w index, __out size_w* pnSize) const
{
	HRESULT hr = S_OK;
	size_w total = 0, spanoffset;

	// find span to start rendering at
	span* sptr = spanfromindex(index, &spanoffset);
	CheckIf(NULL == sptr, E_FAIL);

	// might need to start mid-way through the first span
	spanoffset = index - spanoffset;

	// copy each span's referenced data in succession
	while(sptr != tail)
	{
		size_w cchSpan = sptr->length - spanoffset;
		total += cchSpan;

		sptr = sptr->next;
		spanoffset = 0;
	}

	*pnSize = total;

Cleanup:
	return hr;
}

HRESULT CDataSequence::render_offsets (TArray<size_w>& aOffsets, seqchar_t seqBreak) const
{
	HRESULT hr;
	size_w index = 0;
	span* sptr;

	CheckIf(head == tail, E_FAIL);
	sptr = head->next;

	Check(aOffsets.Append(static_cast<size_w>(0)));

	while(sptr != tail)
	{
		size_w cchSpan = sptr->length;
		seqchar_t* source = buffer_list[sptr->buffer]->buffer + sptr->offset;

		for(size_w i = 0; i < cchSpan; i++)
		{
			if(seqBreak == source[i])
				Check(aOffsets.Append(index + i + 1));
		}
		index += cchSpan;

		sptr = sptr->next;
	}

	// The last offset is included for sizing the real last line.
	Check(aOffsets.Append(index));

Cleanup:
	return hr;
}

size_w CDataSequence::longest_line (seqchar_t seqBreak, seqchar_t seqTab, int nTabWidth)
{
	size_w nLongest = 0;

	if(head != tail)
	{
		size_w xpos = 0;
		span* sptr = head->next;

		while(sptr != tail)
		{
			size_w cchSpan = sptr->length;
			seqchar_t* source = buffer_list[sptr->buffer]->buffer + sptr->offset;

			for(size_w i = 0; i < cchSpan; i++)
			{
				seqchar_t sch = source[i];
				if(seqBreak == sch)
				{
					if(xpos > nLongest)
						nLongest = xpos;
					xpos = 0;
				}
				else if(seqTab == sch)
					xpos += nTabWidth - (xpos % nTabWidth);
				else
					xpos++;
			}

			sptr = sptr->next;
		}

		if(xpos > nLongest)
			nLongest = xpos;
	}

	return nLongest;
}

//
//	CDataSequence::peek
//
//	return single element at specified position in the sequence
//
seqchar_t CDataSequence::peek(size_w index) const
{
	seqchar_t value;
	size_w n;
	return SUCCEEDED(render(index, &value, 1, &n)) ? value : 0;
}

//
//	CDataSequence::poke
//
//	modify single element at specified position in the sequence
//
HRESULT CDataSequence::poke(size_w index, seqchar_t value) 
{
	return replace(index, &value, 1);
}

HRESULT CDataSequence::StreamOut (ISequentialStream* pstmData) const
{
	HRESULT hr = S_OK;
	span* sptr;

	CheckIf(head == tail, E_FAIL);
	sptr = head->next;

	// copy each span's referenced data in succession
	while(sptr != tail)
	{
		ULONG cb;

		size_w cchSpan = sptr->length;
		seqchar_t* source = buffer_list[sptr->buffer]->buffer + sptr->offset;

		Check(pstmData->Write(source, cchSpan * sizeof(seqchar_t), &cb));

		sptr = sptr->next;
	}

Cleanup:
	return hr;
}

//
//	CDataSequence::operator[] const
//
//	readonly array access
//
seqchar_t CDataSequence::operator[] (size_w index) const
{
	return peek(index);
}

//
//	CDataSequence::operator[] 
//
//	read/write array access
//
CDataSequence::ref CDataSequence::operator[] (size_w index)
{
	return ref(this, index);
}

//
//	CDataSequence::breakopt
//
//	Prevent subsequent operations from being optimized (coalesced) 
//  with the last.
//
void CDataSequence::breakopt()
{
	lastaction = action_invalid;
}
