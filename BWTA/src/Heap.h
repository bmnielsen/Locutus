#pragma once
#include <utility>
namespace BWTA
{

  /**
   * An associated heap class with the ability to read and change the values
   * of arbitrary objects in the heap.
   * Note: Objects in the heap must be unique, however any number of objects
   * can have the same value
   */
  template <class _Tp,class _Val> 
  class Heap
  {
    private :
    /** Heap data, stored as a vector */
    std::vector< std::pair< _Tp, _Val > > data;

    /** Maps objects to their positions in the data vector */
    std::map< _Tp, int> mapping;

    /** True if the heap is a min heap, otherwise the heap is a max heap*/
    bool minHeap;

    /**
     * Percolates the given element in the heap up
     * @param the index of the element to percolate up
     * @return the final index of the element, after percolation is complete
     */
    int percolate_up(int index);

    /**
     * Percolates the given element in the heap down
     * @param the index of the element to percolate down
     * @return the final index of the element, after percolation is complete
     */
    int percolate_down(int index);

    public :
    Heap(bool isMinHeap = false) : minHeap(isMinHeap) {}
    ~Heap() {}
    /**
     * Pushes an object associated with the given value onto the heap
     * @param The object-value pair to push onto the heap
     */
    void push(std::pair< _Tp, _Val > x);
    /**
     * Pops the top of the heap
     */
    void pop();
    /**
     * Returns a reference to the object-value pair at the top of the heap
     */
    const std::pair< _Tp, _Val >& top() const;
    /**
     * Updates the given object with the given value and moves the
     * object to the appropiate place in the heap
     * @param an object that exists in the heap
     * @param the new value for that object
     * @return boolean based on the success of the update
     */
    bool set(_Tp& x,_Val& v);
    /**
     * Looks up the given object and returns its value
     * @param object to look up
     * @return the value of the object
     */
    const _Val& get(_Tp& x) const;

    /**
     * Returns true if the heap contains the given element, false otherwise
     */
    bool contains(_Tp& x) const;

    /**
     * Returns true if the heap has no elements, false otherwise
     */
    bool empty() const;
    /**
     * Returns the number of elements in the heap
     */
    int size() const;
    /**
     * Removes all of the elements in the heap
     */
    void clear();
    /**
     * Deletes the given object from the heap
     * @param an object that already exists in the heap
     * @return boolean based on the success of the deletion
     */
    bool erase(_Tp& x);
  };

  //------------------------------- PERCOLATE UP ---------------------------------
  template <class _Tp, class _Val>
  int Heap<_Tp,_Val>::percolate_up(int index)
  {
    if (index<0 || index>=(int)data.size())
      return -1;
    unsigned int parent=(index-1)/2;
    int m=1;
    if (this->minHeap) m=-1;
    while(index>0 && m*data[parent].second<m*data[index].second)
    {
      std::pair<_Tp,_Val> temp=data[parent];
      data[parent]=data[index];
      data[index]=temp;
      (*mapping.find(data[index].first)).second=index;
      index=parent;
      parent=(index-1)/2;
    }
    (*mapping.find(data[index].first)).second=index;
    return index;
  }

  //------------------------------ PERCOLATE DOWN --------------------------------
  template <class _Tp, class _Val>
  int Heap<_Tp,_Val>::percolate_down(int index)
  {
    if (index<0 || index>=(int)data.size())
      return -1;
    unsigned int lchild=index*2+1;
    unsigned int rchild=index*2+2;
    unsigned int mchild;
    int m=1;
    if (this->minHeap) m=-1;
    while((data.size()>lchild && m*data[index].second<m*data[lchild].second) ||
      (data.size()>rchild && m*data[index].second<m*data[rchild].second))
    {
      mchild=lchild;
      if (data.size()>rchild && m*data[rchild].second>m*data[lchild].second)
        mchild=rchild;
      std::pair< _Tp, _Val > temp=data[mchild];
      data[mchild]=data[index];
      data[index]=temp;
      (*mapping.find(data[index].first)).second=index;
      index=mchild;
      lchild=index*2+1;
      rchild=index*2+2;
    }
    (*mapping.find(data[index].first)).second=index;
    return index;
  }
  //----------------------------------- PUSH -------------------------------------
  template <class _Tp, class _Val>
  void Heap<_Tp,_Val>::push(std::pair< _Tp, _Val > x)
  {
    int index=data.size();
    if (mapping.insert(std::make_pair(x.first,index)).second)
    {
      data.push_back(x);
      percolate_up(index);
    }
  }

  //----------------------------------- POP --------------------------------------
  template <class _Tp, class _Val>
  void Heap<_Tp,_Val>::pop()
  {
    if (data.empty())
      return;
    mapping.erase(data.front().first);
    data.front()=data.back();
    data.pop_back();
    if (data.empty())
      return;
    std::map<_Tp,int>::iterator iter=mapping.find(data.front().first);
    if (iter!=mapping.end())
    {
      (*iter).second=0;
      percolate_down(0);
    }
  }

  //----------------------------------- TOP --------------------------------------
  template <class _Tp, class _Val>
  const std::pair< _Tp, _Val >& Heap<_Tp,_Val>::top() const
  {
    return data.front();
  }

  //---------------------------------- EMPTY -------------------------------------
  template <class _Tp, class _Val>
  bool Heap<_Tp,_Val>::empty() const
  {
    return data.empty();
  }

  //----------------------------------- SET --------------------------------------
  template <class _Tp, class _Val>
  bool Heap<_Tp,_Val>::set(_Tp& x,_Val& v)
  {
    std::map<_Tp,int>::iterator iter=mapping.find(x);
    if (iter==mapping.end())
    {
      push(std::make_pair(x,v));
      return true;
    }
    int index=(*iter).second;
    data[index].second=v;
    index=percolate_up(index);
    if (index>=0 && index<(int)data.size())
    {
      percolate_down(index);
      return true;
    }
    return false;
  }

  //----------------------------------- GET --------------------------------------
  template <class _Tp, class _Val>
  const _Val& Heap<_Tp,_Val>::get(_Tp& x) const
  {
    std::map<_Tp,int>::const_iterator iter=mapping.find(x);
    int index=(*iter).second;
    return data[index].second;
  }

  //--------------------------------- CONTAINS -----------------------------------
  template <class _Tp, class _Val>
  bool Heap<_Tp,_Val>::contains(_Tp& x) const
  {
    std::map<_Tp,int>::const_iterator iter=mapping.find(x);
    return (iter!=mapping.end());
  }

  //---------------------------------- SIZE --------------------------------------
  template <class _Tp, class _Val>
  int Heap<_Tp,_Val>::size() const
  {
    return data.size();
  }

  //---------------------------------- CLEAR -------------------------------------
  template <class _Tp, class _Val>
  void Heap<_Tp,_Val>::clear()
  {
    data.clear();
    mapping.clear();
  }

  //---------------------------------- ERASE -------------------------------------
  template <class _Tp, class _Val>
  bool Heap<_Tp,_Val>::erase(_Tp& x)
  {
    std::map<_Tp,int>::iterator iter=mapping.find(x);
    if (iter==mapping.end())
      return false;
    if (data.size()==1)
      this->clear();
    else
    {
      int index=(*iter).second;
      data[index]=data.back();
      data.pop_back();
      mapping.erase(iter);
      percolate_down(index);
    }
    return true;
  }
  //------------------------------------------------------------------------------
}