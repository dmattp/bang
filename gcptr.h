/******************************************************************************
 *    History:      2015-10-07 DMP Initial Creation
 ******************************************************************************/
#ifndef HDR_GCPTR_H_6E131858_6D41_11E5_B707_00FF90F8250A__
#define HDR_GCPTR_H_6E131858_6D41_11E5_B707_00FF90F8250A__








template <class T>
class GCDellocator
{
public:
    static DLLEXPORT void freemem(T*thingmem)
    {
        ::operator delete(thingmem);
    }
};

template <class T>
class GCDeleter
{
public:
    static void deleter( T* thing )
    {
        thing->~T();
        GCDellocator<T>::freemem(thing);
    }
};

    struct Uncopyable
    {
    private:
        Uncopyable( const Uncopyable& );
    public:
        Uncopyable( Uncopyable&& other ) {}
        Uncopyable() {}
    };


    template <class T>
    class gcbase : private Uncopyable
    {
        long refcount_;
        void (*deleter_)( T* );
        //~~~ wait, what?  I don't know that this makes sense either, unless the other guy's
        // refcount is 1 (and is about to be decremented); otherwise people are hanging on to a
        // a moved object, and nobody is necessarily referencing the new object, so what should
        // his refcount be?? .  Maybe it makes sense to allow the rest of the objects content to 
        // be moved while just assuming a 'first initialization' refcount of 1 here.
        // But Let's start with it disabled and see how that goes.
        gcbase( gcbase& other );
        gcbase( gcbase&& other );
        gcbase& operator=( gcbase& other );
        gcbase& operator=( gcbase&& other );
//         {
//             refcount_ = other.refcount_;
//         }
    public:
        gcbase()
        : refcount_(0),
          deleter_( &GCDeleter<T>::deleter )
        {}
        long refcount() const { return refcount_; }
        void ref()
        {
//            ++refcount_;
//            std::cerr << "gcbase=" << this << " ref=" << refcount_ << "\n";
            MT_SAFEISH_INC( refcount_ );
        }
        void unref()
        {
//            std::cerr << "gcbase=" << this << " UNREF=" << refcount_ << "\n";
            if (MT_SAFEISH_DEC( refcount_ ))
            {
                deleter_(static_cast<T*>(this));
            }
        }
    };


    template <class T> // T must derive from gcbase
    class gcptr
    {
    private:
        T* ptr_;
    public:
        gcptr() : ptr_(nullptr) {}
        
        gcptr( T* ptr )
        : ptr_( ptr )
        {
            if (ptr_) ptr_->ref();
        }
        
        gcptr( const gcptr& other )
        : ptr_( other.ptr_ )
        {
            if (ptr_) ptr_->ref();
        }
        
        ~gcptr()
        {
            if (ptr_) ptr_->unref();
        }

        bool operator==( const gcptr<T>& rhs )
        {
            return rhs.ptr_ == ptr_;
        }

#if 1
        gcptr( gcptr&& other )
        : ptr_( other.ptr_ )
        {
            other.ptr_ = nullptr;
        }
        
        gcptr& operator=( gcptr&& other )
        {
            gcptr( static_cast<gcptr&&>(other) ).swap(*this);
            return *this;
        }
#endif
        
        gcptr& operator=( const gcptr& other )
        {
            gcptr(other).swap(*this);
            return *this;
        }
        
        gcptr& operator=( T* other )
        {
            gcptr(other).swap(*this);
            return *this;
        }

        T* get() const       { return ptr_; }
        T& operator*() const { return *ptr_; }
        T* operator->() const { return ptr_; }
        
        operator bool () const { return ptr_ ? true : false; }

        void reset()
        {
            gcptr().swap( *this );
        }
        
        void swap(gcptr & other)
        {
            T * tmp = ptr_;
            ptr_ = other.ptr_;
            other.ptr_ = tmp;
        }
    };

#endif /* ifndef HDR_GCPTR_H_6E131858_6D41_11E5_B707_00FF90F8250A__ */
