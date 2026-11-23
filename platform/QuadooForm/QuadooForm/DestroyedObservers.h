#pragma once

namespace QF
{
	BOOL SameObject(IUnknown* first, IUnknown* second);

	// Used by visual and nonvisual component bases. The caller supplies thread,
	// state and callback lifetime protection; observers may remove one another.
	class CDestroyedObservers
	{
		TArray<IQuadooComponentDestroyedObserver*> m_observers;

	public:
		~CDestroyedObservers()
		{
			for(sysint n = 0; n < m_observers.Length(); n++)
			{
				m_observers[n]->Release();
			}
		}

		HRESULT Add(IQuadooComponentDestroyedObserver* observer)
		{
			HRESULT hr;
			CheckIf(!observer, E_INVALIDARG);
			for(sysint n = 0; n < m_observers.Length(); n++)
			{
				CheckIfIgnore(SameObject(observer, m_observers[n]), S_FALSE);
			}
			Check(m_observers.Append(observer));
			observer->AddRef();
		Cleanup:
			return hr;
		}

		HRESULT Remove(IQuadooComponentDestroyedObserver* observer)
		{
			HRESULT hr = S_FALSE;
			CheckIf(!observer, E_INVALIDARG);
			for(sysint n = 0; n < m_observers.Length(); n++)
			{
				if(SameObject(observer, m_observers[n]))
				{
					IQuadooComponentDestroyedObserver* removed = m_observers[n];
					m_observers.Remove(n, NULL);
					removed->Release();
					hr = S_OK;
					break;
				}
			}
		Cleanup:
			return hr;
		}

		VOID Notify(IQuadooComponent* component)
		{
			while(m_observers.Length())
			{
				TStackRef<IQuadooComponentDestroyedObserver> observer;
				observer.Attach(m_observers[0]);
				m_observers.Remove(0, NULL);
				observer->OnComponentDestroyed(component);
			}
		}
	};
}
