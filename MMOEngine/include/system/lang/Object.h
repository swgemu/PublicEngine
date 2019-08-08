/*
** Copyright (C) 2007-2019 SWGEmu
** See file COPYING for copying conditions.
*/

#ifndef OBJECT_H_
#define OBJECT_H_

#include <atomic>

#include "system/lang/ref/ReferenceCounter.h"
//#include "system/lang/ref/WeakReference.h"
#include "system/lang/ref/StrongAndWeakReferenceCount.h"

#include "system/thread/Mutex.h"

#include "system/util/ArrayList.h"

#include "system/thread/atomic/AtomicBoolean.h"
#include "system/thread/atomic/AtomicReference.h"

#include "Variable.h"

#ifdef MEMORY_PROTECTION
#define REFERENCED_WEAK_MUTEX
#endif

//#define TRACE_REFERENCES

namespace engine {
        namespace stm {
                class TransactionalMemoryManager;
        }
}

namespace sys {
	namespace mm {
		class MemoryManager;
	}

	namespace io {
		class ObjectOutputStream;
		class ObjectInputStream;
	}

	namespace util {
		template<class O> class SortedVector;
		template<class E> class HashSet;
		template<class K, class V> class VectorMap;
	}
}


namespace sys {
  namespace lang {

    class String;
    template<class O> class WeakReference;

	using namespace sys::io;
	using namespace sys::util;

	class Object : public Variable {
		mutable AtomicReference<StrongAndWeakReferenceCount*> referenceCounters;

#ifdef MEMORY_PROTECTION
		AtomicBoolean* _destroying;
#endif

	#ifdef TRACE_REFERENCES
		Mutex referenceMutex;
		sys::util::VectorMap<uint64, StackTrace*>* referenceHolders;
	#endif

	public:
		const static bool is_virtual_object = false;
	public:

		Object();

		Object(const Object& obj);

#ifdef CXX11_COMPILER
		Object(Object&& o) : referenceCounters(nullptr) {
			assert(o.referenceCounters == nullptr); // We cant move objects that are referenced

#ifdef TRACE_REFERENCES
			referenceHolders = nullptr;
#endif
		}
#endif

		~Object();

		Object& operator=(const Object& o) {
			if (this == &o)
				return *this;

			return *this;
		}

#ifdef CXX11_COMPILER
	    Object& operator=(Object&& o) {
		    if (this == &o)
			    return *this;

		    assert(o.referenceCounters == nullptr);

		    return *this;
	    }
#endif
		virtual Object* clone() {
			assert(0 && "clone method not declared");

			return nullptr;
		}

		virtual Object* clone(void* object) {
			return clone();
		}

		virtual int compareTo(Object* object) {
			if (this == object)
				return 0;
			else if (this < object)
				return 1;
			else
				return -1;
		}

		virtual bool notifyDestroy() {
			return true;
		}

		void finalize() {
		}

		bool toBinaryStream(ObjectOutputStream* stream) {
			return false;
		}

		bool parseFromBinaryStream(ObjectInputStream* stream) {
			return false;
		}

		inline void acquire() const {
			if (referenceCounters == nullptr) {
				StrongAndWeakReferenceCount* newCount = new StrongAndWeakReferenceCount(0, 2, const_cast<Object*>(this));

				if (!referenceCounters.compareAndSet(nullptr, newCount)) {
					delete newCount;
				}
			}

			referenceCounters->increaseStrongCount();
		}

		inline bool release() const {
			if (referenceCounters->decrementAndTestAndSetStrongCount() != 0) {
				if (const_cast<Object*>(this)->notifyDestroy()) {
#ifdef WITH_STM
					MemoryManager::getInstance()->reclaim(this);
#else
					const_cast<Object*>(this)->destroy();

					return true;
#endif
				}
			}

			return false;
		}

		inline bool tryFinalRelease() const {
			if (referenceCounters->tryStrongFinalDecrement()) {
				if (const_cast<Object*>(this)->notifyDestroy()) {
#ifdef WITH_STM
					MemoryManager::getInstance()->reclaim(this);
#else
					const_cast<Object*>(this)->destroy();

					return true;
#endif
				}
			}

			return false;
		}

		void _destroyIgnoringCount();

		inline void _markAsDestroyed() {
			if (referenceCounters != nullptr)
				referenceCounters->markAsDestroyed();
		}

		inline uint32 getReferenceCount() {
			if (referenceCounters == nullptr)
				return 0;
			else
				return referenceCounters->getStrongReferenceCount();
		}

		StrongAndWeakReferenceCount* requestWeak() {
			if (referenceCounters == nullptr) {
				StrongAndWeakReferenceCount* newCount = new StrongAndWeakReferenceCount(0, 2, this);

				if (!referenceCounters.compareAndSet(nullptr, newCount))
					delete newCount;
			}

			return (StrongAndWeakReferenceCount*) referenceCounters;
		}

		virtual String toString();

	#ifdef TRACE_REFERENCES
		void addHolder(uint64 obj);

		void removeHolder(uint64 obj);

		void printReferenceHolders();
	#endif

	protected:
		virtual void destroy();

		virtual void free();

		friend class sys::mm::MemoryManager;
		friend class WeakReference<Object*>;
		friend class engine::stm::TransactionalMemoryManager;
	};

  } // namespace lang
} // namespace sys

using namespace sys::lang;

#endif /*OBJECT_H_*/
