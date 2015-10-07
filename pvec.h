/******************************************************************************
 *    History:      2015-10-07 DMP Initial Creation
 ******************************************************************************/
#ifndef HDR_PVEC_H_288D861C_6D44_11E5_B2AC_00FF90F8250A__
#define HDR_PVEC_H_288D861C_6D44_11E5_B2AC_00FF90F8250A__



#include <iostream>
#include <stdexcept>

#include "gcptr.h"

static const int kFanWidth = 4;

//typedef int TValue;

template <class TValue>
class PVect
{
    struct vArray_t : public gcbase<vArray_t>
    {
        TValue values[kFanWidth];
        TValue& operator[]( size_t ndx )
        {
            return values[ndx];
        }
//         vArray_t& operator=( const vArray_t& rhs )
//         {
//             for (int i = 0; i < kFanWidth; ++i)
//                 values[i] = rhs.values[i];
//         }

        inline void copyFrom(  const vArray_t& rhs, int count )
        {
            for (int i = 0; i < count; ++i)
                values[i] = rhs.values[i];
        }
    };

    class Node : public gcbase<Node>
    {
        enum ENodeType {
            kBranchNode,
            kLeafNode,
        };
        struct Leaf {
            gcptr<vArray_t> rf;
            gcptr<vArray_t> lf;
            Leaf() : rf(nullptr), lf(nullptr) {}
            Leaf( const gcptr<vArray_t>& r ) : rf(r), lf(nullptr) {}
            Leaf( const gcptr<vArray_t>& r, const gcptr<vArray_t> l ) : rf(r), lf(l) {}
        };
        struct Branch {
            gcptr<Node> r;
            gcptr<Node> l;
            Branch( const gcptr<Node>& rr, const gcptr<Node>& ll )
            : r(rr), l(ll)
            {}
            Branch( const gcptr<Node>& rr )
            : r(rr)
            {}
            Branch() {}
        };

        Leaf   leaf;
        Branch branch;
        ENodeType nodeType;
        size_t    count;

        void common() {
            std::cout << "new Node\n";
        }
        
    public:
        ~Node()
        {
            std::cout << "delete Node\n";
        }
        Node()
        : nodeType( kLeafNode ),
          count(0)
        {
            common();
        }

        Node( size_t c, const gcptr<vArray_t>& rf )
        : nodeType( kLeafNode ),
          count(c),
          leaf( rf )
        {
            common();
        }
        
        Node( size_t c,  const gcptr<vArray_t>& rf,  const gcptr<vArray_t>& lf )
        : nodeType( kLeafNode ),
          count(c),
          leaf( rf, lf )
        {
            common();
        }

        Node( size_t c,  const gcptr<Node>& rf,  const gcptr<Node>& lf )
        : nodeType( kBranchNode ),
          count(c),
          branch( rf, lf )
        {
            common();
        }

        Node( size_t c,  const gcptr<Node>& rf )
        : nodeType( kBranchNode ),
          count(c),
          branch( rf )
        {
            common();
        }

        bool isfull()
        {
            // from https://graphics.stanford.edu/~seander/bithacks.html#DetermineIfPowerOf2
            // also see http://www.exploringbinary.com/ten-ways-to-check-if-an-integer-is-a-power-of-two-in-c/
            // which suggests complement & compare is faster than decrement and compare, but only trivially so
            const unsigned int v = count; // we want to see if v is a power of 2
            bool f;         // the result goes here 

            f = (v>kFanWidth) && ((v & (v - 1)) == 0);

            return f;
        }

        void walk( int depth = 0 )
        {
            for (int i = 0; i < depth*4; ++i)
                std::cout << ' ';

            std::cout << this << (nodeType==kLeafNode ? " Leaf" : " Branch") << " size=" << count << " full=" << this->isfull() << " refcount=" << this->refcount() << "\n";
            
            if (nodeType == kLeafNode)
            {
                if (leaf.rf)
                    for (int i = 0; i < kFanWidth; ++i)
                    {
                        for (int i = 0; i < (depth*4)+2; ++i)
                            std::cout << ' ';
                        std::cout << (*leaf.rf)[i];
                        std::cout << "\n";
                    }
                    
                if (leaf.lf)
                    for (int i = 0; i < kFanWidth; ++i)
                    {
                        for (int i = 0; i < (depth*4+2); ++i)
                            std::cout << ' ';
                        std::cout << (*leaf.lf)[i];
                        std::cout << "\n";
                    }
            }
            else
            {
                if (branch.r)
                    branch.r->walk( depth + 1 );
                if (branch.l)
                    branch.l->walk( depth + 1 );
            }
        }
        
        gcptr<Node> push( const TValue& t, bool isshared )
        {
            isshared = isshared || (this->refcount() > 1);
                
            if (nodeType == kLeafNode)
            {
                if (count < kFanWidth)
                {
                    if (isshared)
                    {
                        auto newRf = gcptr<vArray_t>(new vArray_t);
                        if (leaf.rf)
                            newRf->copyFrom( *(leaf.rf), count );
                        (*newRf)[count] = t;
                        return new Node( count + 1, newRf );
                    }
                    else 
                    {
                        if (!leaf.rf)
                            leaf.rf = gcptr<vArray_t>(new vArray_t);
                        (*(leaf.rf))[count++] = t;
                        return gcptr<Node>(this);
                    }
                }
                else if (count < kFanWidth*2)
                {
                    if (isshared)
                    {
                        auto newLf = gcptr<vArray_t>(new vArray_t);
                        if (leaf.lf)
                            newLf->copyFrom( *(leaf.lf), count - kFanWidth );
                        (*newLf)[ count - kFanWidth ] = t;
                        return new Node( count+1, leaf.rf, newLf );
                    }
                    else
                    {
                        if (!leaf.lf)
                            leaf.lf = gcptr<vArray_t>(new vArray_t);
                        (*(leaf.lf))[count++ - kFanWidth] = t;
                        return gcptr<Node>(this);
                    }
                }
                else
                {
                    // time to allocate a new node
                    auto newValArray = gcptr<vArray_t>(new vArray_t);
                    (*newValArray)[0] = t;
                    auto newLeaf = gcptr<Node>( new Node( 1, newValArray ) );
                    auto newBranch = gcptr<Node>( new Node( count + 1, gcptr<Node>(this), newLeaf ) );
                    return newBranch;
                }
            }
            else  // allocate on child nodes...
            {
                if (branch.l)
                {
                    if (branch.l->isfull() && (branch.l->size() == branch.r->size()))
                    {
                        auto newValArray = gcptr<vArray_t>(new vArray_t);
                        (*newValArray)[0] = t;
                        auto newLeaf = gcptr<Node>( new Node( 1, newValArray ) );
                        
                        auto newBranch = gcptr<Node>(
                            new Node(
                                count + 1,
                                gcptr<Node>( this ),
                                newLeaf ) );

                        return newBranch;
                    }
                    else
                    {
                        auto newLf = branch.l->push(t, isshared);
                        if (newLf == branch.l)
                        {
                            ++count;
                            return gcptr<Node>( this );
                        }
                        else
                            return gcptr<Node>( new Node( count + 1, branch.r, newLf ) );
                    }
                }
                else
                {
                    const auto& newRf = branch.r->push(t, isshared);
                    if (newRf == branch.r)
                    {
                        ++count;
                        return gcptr<Node>(this);
                    }
                    else
                        return gcptr<Node>( new Node(count + 1, newRf) );
//                    return gcptr<Node>( new Node( count + 1, branch.r->push(t, isshared) ) );
                }
            }
        }
        
        TValue& operator[]( size_t ndx )
        {
            if (ndx >= count)
                throw std::runtime_error( "Access outside of array limits" );
            
            if (nodeType == kLeafNode)
            {
                if (ndx < kFanWidth)
                    return (*leaf.rf)[ndx];
                else
                    return (*leaf.lf)[ndx - kFanWidth];
            }
            else
            {
                const auto rsize = branch.r->size();
                if (ndx < rsize)
                    return (*branch.r)[ndx];
                else
                    return (*branch.l)[ndx-rsize];
            }
        }

        size_t size() const { return count; } 
    }; // end, Node class

    gcptr<Node> root_;
    
public:
    PVect()
    :  root_( new Node )
    {
    }

    size_t size() { return root_->size(); }

    PVect( const gcptr<Node>& root )
    : root_( root )
    {
    }

    PVect push_back( const TValue& v )
    {
        std::cout << "push back\n";
        return PVect( root_->push(v,false) );
    }

    PVect& operator=( const PVect& rhs )
    {
        root_ = rhs.root_;
    }

    TValue& operator[]( size_t ndx )
    {
        return (*root_)[ndx];
    }

    void walk() { root_->walk(); }
};


#endif /* ifndef HDR_PVEC_H_288D861C_6D44_11E5_B2AC_00FF90F8250A__ */
