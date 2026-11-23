#include <windows.h>
#include "Published\QuadooParser.h"
#include "FormSourceSync.h"

namespace StudioSource
{
	static INT s_failure = -1;
	VOID FailAfter(INT checkpoint) { s_failure = checkpoint; }

	HRESULT Checkpoint()
	{
		if(s_failure < 0) return S_OK;
		if(!s_failure--) { s_failure = -1; return E_OUTOFMEMORY; }
		return S_OK;
	}

	static BOOL Letter(WCHAR ch) { return ch == L'_' || (ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z') || ch >= 256; }
	static BOOL Digit(WCHAR ch) { return ch >= L'0' && ch <= L'9'; }

	HRESULT ValidateIdentifier (RSTRING name)
	{
		HRESULT hr;
		BOOL valid;

		CheckIf(!name || !RStrIsWide(name),E_INVALIDARG);
		Check(QuadooIsValidIdentifier(RStrToWide(name),RStrLen(name),&valid));
		CheckIfIgnore(!valid,E_INVALIDARG);

	Cleanup:
		return hr;
	}

	HRESULT ReadDocument (ITextDocument* document, RSTRING* text)
	{
		HRESULT hr;
		CMemoryStream stream;
			CheckIf(!document || !text,E_POINTER);
		*text = NULL;
			Check(document->StreamOut(&stream));
			CheckIf(stream.TDataRemaining<WCHAR>() > 8 * 1024 * 1024,E_INVALIDARG);
			Check(RStrCreateW(static_cast<INT>(stream.TDataRemaining<WCHAR>()),stream.TGetReadPtr<WCHAR>(),text));
	Cleanup:
		return hr;
	}

	struct Token
	{
		INT start, length, pair;
		WCHAR symbol;
		Token() : start(0), length(0), pair(-1), symbol(0) {}
	};

	struct Scope
	{
		INT declaration, open, close;
		Scope() : declaration(-1), open(-1), close(-1) {}
	};

	struct Region
	{
		INT start, end, body, bodyEnd;
		Region() : start(-1), end(-1), body(-1), bodyEnd(-1) {}
		BOOL Contains(INT offset) const { return start >= 0 && offset >= start && offset < end; }
	};

	struct Member
	{
		CRString name;
		BOOL method, compatible;
		INT offset;
		Member() : method(FALSE), compatible(FALSE), offset(-1) {}
	};

	class CScanner
	{
		PCWSTR m_text;
		INT m_length;
	public:
		TArray<Token> tokens, comments;
		CScanner(RSTRING text) : m_text(RStrToWide(text)),m_length(RStrLen(text)) {}
		BOOL Is(INT n, PCWSTR word) const
		{
			INT count = TStrLenAssert(word);
			return n >= 0 && n < tokens.Length() && tokens[n].length == count && !memcmp(m_text + tokens[n].start,word,count * sizeof(WCHAR));
		}
		HRESULT Text(INT n, RSTRING* text) const { return RStrCreateW(tokens[n].length,m_text + tokens[n].start,text); }
		HRESULT Scan()
		{
			HRESULT hr = S_OK;
			TArray<INT> stack;
			for(INT n = 0; n < m_length;)
			{
				Token token; token.start = n;
				WCHAR ch = m_text[n];
				if(ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n' || ch == 0xfeff) { n++; continue; }
				if(ch == L'/' && n + 1 < m_length && (m_text[n + 1] == L'/' || m_text[n + 1] == L'*'))
				{
					BOOL line = m_text[n + 1] == L'/'; n += 2;
					if(line) { while(n < m_length && m_text[n] != L'\r' && m_text[n] != L'\n') n++; }
					else
					{
						while(n + 1 < m_length && !(m_text[n] == L'*' && m_text[n + 1] == L'/')) n++;
							CheckIf(n + 1 >= m_length,HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
						n += 2;
					}
					token.length = n - token.start;
						Check(comments.Append(token));
					continue;
				}
				if(ch == L'#')
				{
					INT directive, end;
					n++;
					while(n < m_length && (m_text[n] == L' ' || m_text[n] == L'\t'))
					{
						n++;
					}
					directive = n;
					while(n < m_length && m_text[n] != L'\r' && m_text[n] != L'\n')
					{
						n++;
					}
					end = n;
					while(end > directive && (m_text[end - 1] == L' ' || m_text[end - 1] == L'\t'))
					{
						end--;
					}
					// Pragma once and includes do not alter lexical ownership.
					BOOL include = directive + 7 < end && !memcmp(m_text + directive,L"include",7 * sizeof(WCHAR)) &&
						(m_text[directive + 7] == L' ' || m_text[directive + 7] == L'\t' || m_text[directive + 7] == L'"');
					BOOL once = end - directive == 11 && !memcmp(m_text + directive,L"pragma once",11 * sizeof(WCHAR));
					CheckIf(!include && !once,HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
					continue;
				}
				if(ch == L'"' || ch == L'\'')
				{
					n++; BOOL closed = FALSE;
					while(n < m_length)
					{
						WCHAR next = m_text[n++];
						if(next == L'\\') {
								CheckIf(n >= m_length,HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
						n++; }
						else if(next == ch) { closed = TRUE; break; }
					}
						CheckIf(!closed,HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
					token.symbol = L'"';
				}
				else if(Letter(ch))
				{
					n++;
					while(n < m_length && (Letter(m_text[n]) || Digit(m_text[n])))
						n++;
				}
				else
				{
					token.symbol = ch;
					n++;
				}
				token.length = n - token.start;
				INT index = static_cast<INT>(tokens.Length());
					Check(tokens.Append(token));
				if(ch == L'{' || ch == L'(' || ch == L'[')
					Check(stack.Append(index));
				else if(ch == L'}' || ch == L')' || ch == L']')
				{
					CheckIf(!stack.Length(),HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
					INT open = stack[stack.Length() - 1]; WCHAR start = tokens[open].symbol;
						CheckIf((ch == L'}' && start != L'{') || (ch == L')' && start != L'(') || (ch == L']' && start != L'['),HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
					tokens[open].pair = index; tokens[index].pair = open; stack.Remove(stack.Length() - 1,NULL);
				}
			}
			CheckIf(stack.Length(),HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
		Cleanup:
			return hr;
		}

		HRESULT FindScope(PCWSTR kind, RSTRING name, Scope& scope)
		{
			HRESULT hr = S_OK;
			for(INT n = 0; n + 1 < tokens.Length(); n++)
			{
				if(tokens[n].symbol == L'{') { n = tokens[n].pair; continue; }
				if(!Is(n,kind)) continue;
				if(!Is(n + 1,RStrToWide(name))) continue;
					CheckIf(scope.open >= 0,QuadooForm::EDuplicateName);
				scope.declaration = n;
				for(INT p = n + 2; p < tokens.Length(); p++)
				{
					if(tokens[p].symbol == L'(' || tokens[p].symbol == L'[') { p = tokens[p].pair; continue; }
					if(tokens[p].symbol == L'{') { scope.open = p; scope.close = tokens[p].pair; break; }
						CheckIf(tokens[p].symbol == L';',HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
				}
					CheckIf(scope.open < 0,HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
			}
		Cleanup:
			return hr;
		}

		HRESULT FindRegion(const Scope& scope, PCWSTR kind, REFGUID id, Region& region)
		{
			HRESULT hr;
			WCHAR guid[40]; CRString begin, end;
				CheckIf(!StringFromGUID2(id,guid,ARRAYSIZE(guid)),E_FAIL);
				Check(RStrFormatW(&begin,L"// <QuadooStudio:%ls %ls>",kind,guid));
				Check(RStrFormatW(&end,L"// </QuadooStudio:%ls>",kind));
			for(sysint n = 0; n < comments.Length(); n++)
			{
				const Token& comment = comments[n];
				if(comment.length >= 18 && !memcmp(m_text + comment.start,L"// <QuadooStudio:",17 * sizeof(WCHAR)))
				{
					// All ownership markers must identify this unit and region exactly.
					if(comment.length == RStrLen(begin) && !memcmp(m_text + comment.start,begin.c_wstr(),comment.length * sizeof(WCHAR)))
					{
							CheckIf(region.start >= 0,HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
						region.start = comment.start;
						while(region.start > 0 && (m_text[region.start - 1] == L' ' || m_text[region.start - 1] == L'\t')) region.start--;
						region.body = comment.start + comment.length;
					}
				}
				if(comment.length == RStrLen(end) && !memcmp(m_text + comment.start,end.c_wstr(),comment.length * sizeof(WCHAR)))
				{
						CheckIf(region.start < 0 || region.end >= 0,HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
					region.bodyEnd = comment.start; while(region.bodyEnd > 0 && (m_text[region.bodyEnd - 1] == L' ' || m_text[region.bodyEnd - 1] == L'\t')) region.bodyEnd--;
					region.end = comment.start + comment.length;
					if(region.end < m_length && m_text[region.end] == L'\r') region.end++;
					if(region.end < m_length && m_text[region.end] == L'\n') region.end++;
				}
			}
			CheckIf((region.start >= 0) != (region.end >= 0),HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
			if(region.start >= 0)
			{
				CheckIf(scope.open < 0 || region.start <= tokens[scope.open].start || region.end > tokens[scope.close].start,HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
				for(INT n = scope.open + 1; n < scope.close && tokens[n].start < region.start; n++)
				{
					CheckIf(tokens[n].pair > n && tokens[tokens[n].pair].start >= region.start,HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
				}
			}
		Cleanup:
			return hr;
		}

		HRESULT Members(const Scope& scope, const Region& owned, TSourceObjects<Member>& members)
		{
			HRESULT hr = S_OK;
			if(scope.open < 0) goto Cleanup;
			for(INT n = scope.open + 1; n < scope.close;)
			{
				if(owned.Contains(tokens[n].start)) { n++; continue; }
				if(tokens[n].symbol == L';') { n++; continue; }
				INT end = n;
				BOOL initializer = FALSE;
				while(end < scope.close && tokens[end].symbol != L';')
				{
					if(tokens[end].symbol == L'=') initializer = TRUE;
					if(tokens[end].symbol == L'{' && !initializer) break;
					if(tokens[end].symbol == L'(' || tokens[end].symbol == L'[' || tokens[end].symbol == L'{') end = tokens[end].pair;
					end++;
				}
					CheckIf(end >= scope.close,HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
				BOOL method = FALSE, virt = FALSE, callable = FALSE, isStatic = FALSE, assignment = FALSE;
				for(INT p = n; p < end; p++)
				{
					if(tokens[p].symbol == L'=') assignment = TRUE;
					if(Is(p,L"virtual")) virt = TRUE;
					if(Is(p,L"function")) callable = TRUE;
					if(Is(p,L"static")) isStatic = TRUE;
					if(tokens[p].symbol == L'(')
					{
						if(!assignment && p > n && !tokens[p - 1].symbol && !Is(p - 1,L"extern") && !Is(p - 1,L"function"))
						{
							Member member; member.method = TRUE; member.offset = tokens[p - 1].start;
								Check(Text(p - 1,&member.name));
							member.compatible = virt && callable && !isStatic && tokens[p].pair == p + 2 && !tokens[p + 1].symbol;
								Check(members.Append(member));
							method = TRUE; break;
						}
						p = tokens[p].pair;
					}
				}
				if(!method)
				{
					INT p = n;
					if(Is(p,L"static")) p++;
					if(Is(p,L"property")) p++;
					if(Is(p,L"extern")) p++;
					if(Is(p,L"var") || Is(p,L"int") || Is(p,L"long") || Is(p,L"float") || Is(p,L"double") || Is(p,L"bool") || Is(p,L"string")) p++;
					if(p < end && !tokens[p].symbol)
					{
						Member member; member.offset = tokens[p].start;
							Check(Text(p,&member.name));
							Check(members.Append(member));
						for(INT q = p + 1; q + 1 < end; q++)
						{
							if(tokens[q].symbol == L'(' || tokens[q].symbol == L'[' || tokens[q].symbol == L'{') { q = tokens[q].pair; continue; }
							if(tokens[q].symbol == L',' && !tokens[q + 1].symbol) { Member extra; extra.offset = tokens[q + 1].start;
									Check(Text(q + 1,&extra.name));
									Check(members.Append(extra));
							}
						}
					}
				}
				n = tokens[end].symbol == L'{' ? tokens[end].pair + 1 : end + 1;
			}
		Cleanup:
			return hr;
		}
	};

	struct Component
	{
		GUID id;
		CRString name;
	};

	static HRESULT Field(IJSONObject* object, PCWSTR name, RSTRING* text)
	{
		HRESULT hr; TStackRef<IJSONValue> value;
		Check(object->FindNonNullValueW(name,&value));
		Check(value->GetString(text));
	Cleanup:
		return hr;
	}

	static HRESULT Collect(IJSONObject* object, BOOL root, TSourceObjects<Component>& components, TSourceObjects<CRString>& handlers, UINT depth = 0)
	{
		HRESULT hr;
		TStackRef<IJSONValue> value;
		CRString rootName;
			CheckIf(depth > 128,E_INVALIDARG);
		if(root)
		{
			Check(Field(object,L"name",&rootName));
			Check(ValidateIdentifier(rootName));
		}
		if(!root)
		{
			Component component;
			Check(Field(object,L"name",&component.name));
			Check(ValidateIdentifier(component.name));
			Check(object->FindNonNullValueW(L"id",&value));
			Check(value->GetStringAsGuid(component.id));
			value.Release();
			for(sysint n = 0; n < components.Length(); n++)
			{
				CheckIf(components[n].name == static_cast<RSTRING>(component.name) || IsEqualGUID(components[n].id,component.id),QuadooForm::EDuplicateName);
			}
			Check(components.Append(component));
		}
		if(object->HasField(RSTRING_CAST(L"events")))
		{
			TStackRef<IJSONObject> events;
			Check(object->FindNonNullValueW(L"events",&value));
			Check(value->GetObject(&events));
			value.Release();
			for(sysint n = 0; n < events->Count(); n++)
			{
				CRString name; BOOL found = FALSE;
				Check(events->GetValueByIndex(n,&value));
				Check(value->GetString(&name));
				value.Release();
				if(!RStrLen(name)) continue;
					Check(ValidateIdentifier(name));
				for(sysint p = 0; p < handlers.Length(); p++) if(handlers[p] == static_cast<RSTRING>(name)) found = TRUE;
				if(!found)
					Check(handlers.Append(name));
			}
		}
		for(UINT f = 0; f < 2; f++)
		{
			PCWSTR field = f ? L"components" : L"children";
			if(!object->HasField(RSTRING_CAST(field))) continue;
			TStackRef<IJSONArray> children;
				Check(object->FindNonNullValueW(field,&value));
				Check(value->GetArray(&children));
			value.Release();
			for(sysint n = 0; n < children->Count(); n++)
			{
				TStackRef<IJSONObject> child;
				Check(children->GetObject(n,&child));
				Check(Collect(child,FALSE,components,handlers,depth + 1));
			}
		}
		hr = S_OK;
	Cleanup:
		return hr;
	}

	static HRESULT Write(CMemoryStream& stream, PCWSTR text)
	{
		ULONG written;
		return stream.Write(text,TStrLenAssert(text) * sizeof(WCHAR),&written);
	}

	static HRESULT StreamText(CMemoryStream& stream, RSTRING* text)
	{
		return RStrCreateW(static_cast<INT>(stream.TDataRemaining<WCHAR>()),stream.TGetReadPtr<WCHAR>(),text);
	}

	static HRESULT AddEdit(Result& result, INT offset, INT length, RSTRING text)
	{
		HRESULT hr;
		Edit edit;
		edit.offset = offset;
		edit.length = length;
		edit.text = text;
		Check(result.edits.Append(edit));
	Cleanup:
		return hr;
	}

	static HRESULT OwnedRegion(RSTRING source, IJSONObject* previous, PCWSTR field, const Region& region)
	{
		HRESULT hr = S_OK;
		CRString expected;
		if(previous)
		{
			Check(Field(previous,field,&expected));
			CheckIf(region.start < 0 || region.end - region.start != RStrLen(expected) || memcmp(RStrToWide(source) + region.start,expected.c_wstr(),RStrLen(expected) * sizeof(WCHAR)),HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH));
		}
		else
		{
			CheckIf(region.start >= 0,HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH));
		}
	Cleanup:
		return hr;
	}

	static HRESULT RegionText(PCWSTR kind, REFGUID id, PCWSTR body, PCWSTR newline, RSTRING* result)
	{
		HRESULT hr; WCHAR guid[40];
		CheckIf(!StringFromGUID2(id,guid,ARRAYSIZE(guid)),E_FAIL);
		Check(RStrFormatW(result,L"\t// <QuadooStudio:%ls %ls>%ls%ls\t// </QuadooStudio:%ls>%ls",kind,guid,newline,body,kind,newline));
	Cleanup:
		return hr;
	}

	HRESULT Prepare(RSTRING source, RSTRING className, REFGUID unit, IJSONObject* metadata,
		IJSONObject* definition, RSTRING createHandler, Result& result, BOOL repair)
	{
		HRESULT hr;
		CScanner scan(source);
		Scope script, events;
		Region membersRegion, eventsRegion;
		TSourceObjects<Member> members, declarations;
		TSourceObjects<Component> components;
		TSourceObjects<CRString> handlers;
		TStackRef<IJSONObject> previous, sync;
		TStackRef<IJSONValue> value, clone;
		TStackRef<IJSONArray> mapping;
		CRString interfaceName, memberBody, eventBody, memberText, eventText, missing, externalMember;
		CMemoryStream memberStream, eventStream;
		PCWSTR newline = TStrChr(RStrToWide(source),L'\r') ? L"\r\n" : L"\n";
		BOOL inherits = FALSE, needsHandler = RStrLen(createHandler) != 0;

		Check(Checkpoint());
		Check(ValidateIdentifier(className));
		Check(RStrFormatW(&interfaceName,L"I%ls",RStrToWide(className)));
		Check(scan.Scan());
		Check(scan.FindScope(L"class",className,script));
		CheckIf(script.open < 0,HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
		for(INT n = script.declaration + 2; n + 2 < script.open; n++)
		{
			if(scan.Is(n,L"extern") && scan.tokens[n + 1].symbol == L'(')
			{
				Check(scan.Text(n + 2,&externalMember));
				CheckIf(!(externalMember == RSTRING_CAST(L"Form")) && !(externalMember == RSTRING_CAST(L"Frame")),E_INVALIDARG);
				break;
			}
		}
		Check(scan.FindScope(L"interface",interfaceName,events));
		Check(scan.FindRegion(script,L"members",unit,membersRegion));
		Check(scan.FindRegion(events,L"events",unit,eventsRegion));

		if(metadata->HasField(RSTRING_CAST(L"sourceSync")))
		{
			Check(metadata->FindNonNullValueW(L"sourceSync",&value));
			Check(value->GetObject(&previous));
			value.Release();
			INT version;
			Check(previous->FindNonNullValueW(L"version",&value));
			Check(value->GetInteger(&version));
			value.Release();
			CheckIf(version != 1,HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH));
		}
		if(!repair)
		{
			Check(OwnedRegion(source,previous,L"membersText",membersRegion));
			Check(OwnedRegion(source,previous,L"eventsText",eventsRegion));
		}
		Check(scan.Members(script,membersRegion,members));
		Check(scan.Members(events,eventsRegion,declarations));
		for(sysint n = 0; n < members.Length(); n++)
		{
			CheckIf(members[n].name == RSTRING_CAST(L"__QuadooUnitId") || members[n].name == RSTRING_CAST(L"__QuadooRoot"),QuadooForm::EDuplicateName);
			for(sysint p = 0; p < n; p++)
			{
				CheckIf(members[n].name == static_cast<RSTRING>(members[p].name),QuadooForm::EDuplicateName);
			}
		}
		for(sysint n = 0; n < declarations.Length(); n++)
		{
			for(sysint p = 0; p < n; p++)
			{
				CheckIf(declarations[n].name == static_cast<RSTRING>(declarations[p].name),QuadooForm::EDuplicateName);
			}
		}

		Check(Collect(definition,TRUE,components,handlers));
		Check(JSONCreateObject(&sync));
		Check(QFD::PutInt(sync,L"version",1));
		Check(JSONCreateArray(&mapping));

		{
			WCHAR guid[40]; CRString identity;
			CheckIf(!StringFromGUID2(unit,guid,ARRAYSIZE(guid)),E_FAIL);
			Check(RStrFormatW(&identity,L"\tproperty __QuadooUnitId { get { return \"%ls\"; } }%ls",guid,newline));
			Check(Write(memberStream,identity.c_wstr()));
		}
		if(RStrLen(externalMember))
		{
			CRString access;
			BOOL constructor = FALSE;
			Check(RStrFormatW(&access,L"\tproperty __QuadooRoot%ls\t{%ls\t\tget%ls\t\t{%ls\t\t\treturn %ls;%ls\t\t}%ls\t\tset (value)%ls\t\t{%ls\t\t\t%ls = value;%ls\t\t}%ls\t}%ls",newline,newline,newline,newline,externalMember.c_wstr(),newline,newline,newline,newline,externalMember.c_wstr(),newline,newline,newline));
			Check(Write(memberStream,access.c_wstr()));
			for(sysint n = 0; n < members.Length(); n++)
			{
				if(members[n].method && members[n].name == className)
				{
					constructor = TRUE;
				}
			}
			if(!constructor)
			{
				CRString kind, body;
				Check(Field(definition,L"type",&kind));
				BOOL frame = kind == RSTRING_CAST(L"Frame");
				Check(RStrFormatW(&body,L"%ls\t%ls (%ls)%ls\t{%ls\t\t%ls = %ls.Create(this%ls);%ls\t}%ls",newline,RStrToWide(className),frame ? L"Owner" : L"",newline,newline,externalMember.c_wstr(),externalMember.c_wstr(),frame ? L", Owner" : L"",newline,newline));
				Check(AddEdit(result,scan.tokens[script.close].start,0,body));
			}
		}

		for(sysint n = 0; n < components.Length(); n++)
		{
			const Component& component = components[n];
			CRString line, identity; WCHAR guid[40]; TStackRef<IJSONObject> entry;
			CheckIf(component.name == className || component.name == static_cast<RSTRING>(interfaceName) || component.name == RSTRING_CAST(L"Form") || component.name == RSTRING_CAST(L"Frame") || component.name == RSTRING_CAST(L"__QuadooUnitId") || component.name == RSTRING_CAST(L"__QuadooRoot"),QuadooForm::EDuplicateName);
			for(sysint p = 0; p < members.Length(); p++)
			{
				CheckIf(members[p].name == static_cast<RSTRING>(component.name),QuadooForm::EDuplicateName);
			}

			CheckIf(!StringFromGUID2(component.id,guid,ARRAYSIZE(guid)),E_FAIL);
			Check(RStrCreateW(TStrLenAssert(guid),guid,&identity));
			Check(RStrFormatW(&line,L"\tproperty var %ls; // %ls%ls",component.name.c_wstr(),guid,newline));
			Check(Write(memberStream,line.c_wstr()));
			Check(JSONCreateObject(&entry));
			Check(QFD::PutString(entry,L"id",identity));
			Check(QFD::PutString(entry,L"member",component.name));
			Check(JSONWrapObject(entry,&value));
			Check(mapping->Add(value));
			value.Release();
		}

		for(sysint n = 0; n < handlers.Length(); n++)
		{
			BOOL declared = FALSE, implemented = FALSE;
			for(sysint p = 0; p < components.Length(); p++)
			{
				CheckIf(components[p].name == static_cast<RSTRING>(handlers[n]),QuadooForm::EDuplicateName);
			}
			for(sysint p = 0; p < members.Length(); p++) if(members[p].name == static_cast<RSTRING>(handlers[n]))
			{
				CheckIf(!members[p].method || !members[p].compatible,DISP_E_TYPEMISMATCH);
				implemented = TRUE;
			}
			for(sysint p = 0; p < declarations.Length(); p++) if(declarations[p].name == static_cast<RSTRING>(handlers[n]))
			{
				CheckIf(!declarations[p].method || !declarations[p].compatible,DISP_E_TYPEMISMATCH);
				declared = TRUE;
			}
			if(!declared)
			{
				CRString line;
				Check(RStrFormatW(&line,L"\tvirtual function %ls (oArgs) = 0;%ls",handlers[n].c_wstr(),newline));
				Check(Write(eventStream,line.c_wstr()));
			}
			if(!implemented && !(createHandler && handlers[n] == createHandler) && !RStrLen(missing))
				missing = handlers[n];
		}
		if(needsHandler)
		{
			BOOL bound = FALSE;
				Check(ValidateIdentifier(createHandler));
			for(sysint n = 0; n < handlers.Length(); n++)
			{
				if(handlers[n] == createHandler) bound = TRUE;
					CheckIf(!bound,E_INVALIDARG);
			}
			for(sysint n = 0; n < members.Length(); n++)
			{
				if(members[n].name == createHandler)
				{
					CheckIf(!members[n].compatible,DISP_E_TYPEMISMATCH);
					needsHandler = FALSE; result.handlerOffset = members[n].offset;
				}
			}
			if(needsHandler)
			{
				CRString body;
				Check(RStrFormatW(&body,L"%ls\tvirtual function %ls (oArgs)%ls\t{%ls\t\treturn true;%ls\t}%ls",newline,RStrToWide(createHandler),newline,newline,newline,newline));
				Check(AddEdit(result,scan.tokens[script.close].start,0,body));
			}
		}
			Check(StreamText(memberStream,&memberBody));
			Check(StreamText(eventStream,&eventBody));
			Check(RegionText(L"members",unit,memberBody.c_wstr(),newline,&memberText));
			Check(RegionText(L"events",unit,eventBody.c_wstr(),newline,&eventText));
		if(membersRegion.start < 0) { CRString text;
				Check(RStrFormatW(&text,L"%ls%ls",newline,memberText.c_wstr()));
				Check(AddEdit(result,scan.tokens[script.open].start + 1,0,text));
		}
		else if(membersRegion.end - membersRegion.start != RStrLen(memberText) || memcmp(RStrToWide(source) + membersRegion.start,memberText.c_wstr(),RStrLen(memberText) * sizeof(WCHAR))) {
				Check(AddEdit(result,membersRegion.start,membersRegion.end - membersRegion.start,memberText));
		}
		for(INT n = script.declaration + 2; n < script.open; n++) if(scan.Is(n,interfaceName.c_wstr())) inherits = TRUE;
		if(!inherits)
		{
			BOOL colon = FALSE; CRString text;
			for(INT n = script.declaration + 2; n < script.open; n++) if(scan.tokens[n].symbol == L':') colon = TRUE;
				Check(RStrFormatW(&text,L"%ls%ls ",colon ? L", " : L": ",interfaceName.c_wstr()));
				Check(AddEdit(result,scan.tokens[script.open].start,0,text));
		}
		if(events.open < 0)
		{
			CRString text;
				Check(RStrFormatW(&text,L"interface %ls%ls{%ls%ls};%ls%ls",interfaceName.c_wstr(),newline,newline,eventText.c_wstr(),newline,newline));
				Check(AddEdit(result,scan.tokens[script.declaration].start,0,text));
		}
		else if(eventsRegion.start < 0) { CRString text;
				Check(RStrFormatW(&text,L"%ls%ls",newline,eventText.c_wstr()));
				Check(AddEdit(result,scan.tokens[events.open].start + 1,0,text));
		}
		else if(eventsRegion.end - eventsRegion.start != RStrLen(eventText) || memcmp(RStrToWide(source) + eventsRegion.start,eventText.c_wstr(),RStrLen(eventText) * sizeof(WCHAR))) {
				Check(AddEdit(result,eventsRegion.start,eventsRegion.end - eventsRegion.start,eventText));
		}
			Check(QFD::PutString(sync,L"membersText",memberText));
			Check(QFD::PutString(sync,L"eventsText",eventText));
			Check(JSONWrapArray(mapping,&value));
			Check(sync->AddValueW(L"members",value));
		value.Release();
			Check(JSONWrapObject(metadata,&value));
			Check(JSONClone(value,&clone,TRUE));
			Check(clone->GetObject(&result.metadata));
		value.Release();
			Check(JSONWrapObject(sync,&value));
			Check(result.metadata->AddValueW(L"sourceSync",value));
		// Sort descending; at equal offsets retain insertion order.
		for(sysint n = 1; n < result.edits.Length(); n++) for(sysint p = n; p > 0 && result.edits[p].offset > result.edits[p - 1].offset; p--) SwapData(result.edits[p],result.edits[p - 1]);
		{
			CMemoryStream text; INT cursor = 0; ULONG written;
			for(sysint n = result.edits.Length(); n > 0; n--)
			{
				const Edit& edit = result.edits[n - 1];
					CheckIf(edit.offset < cursor || edit.offset + edit.length > RStrLen(source),HRESULT_FROM_WIN32(ERROR_BAD_FORMAT));
					Check(text.Write(RStrToWide(source) + cursor,(edit.offset - cursor) * sizeof(WCHAR),&written));
					Check(Write(text,edit.text.c_wstr()));
				cursor = edit.offset + edit.length;
			}
				Check(text.Write(RStrToWide(source) + cursor,(RStrLen(source) - cursor) * sizeof(WCHAR),&written));
				CheckIf(text.TDataRemaining<WCHAR>() > 8 * 1024 * 1024,E_INVALIDARG);
				Check(StreamText(text,&result.text));
		}
		if(RStrLen(missing)) {
				Check(RStrFormatW(&result.diagnostic,L"Handler %ls is not implemented. Use Create/Open Handler in Events.",missing.c_wstr()));
		}
		if(needsHandler)
		{
			CScanner updated(result.text); Scope target; Region owned; TSourceObjects<Member> after;
				Check(updated.Scan());
				Check(updated.FindScope(L"class",className,target));
				Check(updated.FindRegion(target,L"members",unit,owned));
				Check(updated.Members(target,owned,after));
			for(sysint n = 0; n < after.Length(); n++) if(after[n].name == createHandler) result.handlerOffset = after[n].offset;
		}
		else if(result.handlerOffset >= 0) for(sysint n = 0; n < result.edits.Length(); n++) if(result.edits[n].offset <= result.handlerOffset) result.handlerOffset += RStrLen(result.edits[n].text) - result.edits[n].length;
			Check(Checkpoint());
	Cleanup:
		if(FAILED(hr) && !RStrLen(result.diagnostic)) RStrFormatW(&result.diagnostic,L"Source synchronization failed (0x%.8x). Check identifiers, conflicting declarations and QuadooStudio managed regions.",hr);
		return hr;
	}
}
