#pragma once

namespace BWTA
{
  /**
   * Template used for work with dynamically initialized array with dimension 2.
   */
  template <class Type>
  class RectangleArray
   {
     public :
       /**
        * Creates the array with the specified proportions.
        * @param width Width of the new array.
        * @param height Height of the new array.
        */
       RectangleArray(unsigned int width = 1, unsigned int height = 1, Type* data = NULL);
       /** Copy constructor */
       RectangleArray(const RectangleArray<Type>& rectangleArray);
	   /** Assignment operator */
	   const RectangleArray& operator=(const RectangleArray<Type>& rectangleArray);
       /** Destroys the array and deletes all content of array. */
       ~RectangleArray(void);
       /**
        * Gets the width of the array.
        * @return width of the array.
        */
       unsigned int getWidth(void) const;
       /**
        * Gets the height of the array.
        * @return height of the array.
        */
       unsigned int getHeight(void) const;
       /**
        * Gets item of the array on the specified position.
        * @param x horizontal index of the array position.
        * @param y vertical index of the array position.
        * @return item on the specified position.
        */
       Type getItem(unsigned int x, unsigned int y);
       Type getItemSafe(unsigned int x, unsigned int y);
       inline Type* operator[](int i) { return this->getColumn(i); }
       inline Type const * const operator[](int i) const {return this->getColumn(i); }
       /**
        * Sets item of the array on the specified position.
        * @param x horizontal index of the array position.
        * @param y vertical index of the array position.
        * @param item new value of the field.
        */
       void setItem(unsigned int x, unsigned int y, Type *item);
       void resize(unsigned int width, unsigned int height);
       void saveToFile(const std::string& fileName);
       /** Sets all fields of the array to the specified value */
       void setTo(const Type& value);
       void setBorderTo(const Type& value);
	   /** Set a rectangle area of the array to the specified values */
	   void setRectangleTo(unsigned int xLeft, unsigned int yTop, unsigned int xRight, unsigned int yBottom, const Type& value);
     private :
       bool owner;
       /** width of array */
       unsigned int width;
       /** height of array */
       unsigned int height;
       /** Array data, stored as linear array of size width*height */
       Type *data;
       /** Pointers to begins of lines*/
       Type **columns;
       /**
        * Gets data item on the specified index
        * @param index index of the data to be returned.
        */
       Type getData(unsigned int index);
       /**
        * Gets the pointer in data to the beginning of line with the specified
        * index.
        * @param index index of the line.
        */
       Type *getColumn(unsigned int index);
       /**
        * Gets the pointer in data to the beginning of line with the specified
        * index.
        * @param index index of the line.
        */
       const Type *getColumn(unsigned int index) const;
       /**
        * Sets the width of the array.
        * @param width New width of the array.
        */
       void setWidth(unsigned int width);
       /**
        * Sets the height of the array.
        * @param height New height of the array.
        */
       void setHeight(unsigned int height);       
   };
  //---------------------------------------------- CONSTRUCTOR -----------------------------------------------
  template <class Type>
  RectangleArray<Type>::RectangleArray(unsigned int width, unsigned int height, Type* data)
  {
    this->setWidth(width);
    this->setHeight(height);
    this->owner = (data == NULL);
    if (this->owner)
      this->data = new Type[this->getWidth()*this->getHeight()];
    else
      this->data = data;

    columns = new Type*[this->getWidth()];
    unsigned int i = 0;
    for (unsigned int position = 0;i < width; i ++,position += height)
      columns[i] = &this->data[position];
  }
  //-------------------------------------------- COPY CONSTRUCTOR --------------------------------------------
  template <class Type>
  RectangleArray<Type>::RectangleArray(const RectangleArray<Type>& rectangleArray)
  :owner(true)
  {
    this->setWidth(rectangleArray.getWidth());
    this->setHeight(rectangleArray.getHeight());
    this->data = new Type[this->getWidth()*this->getHeight()];
    columns = new Type*[this->getWidth()];
    
    unsigned int i = 0;
    for (unsigned int position = 0;i < width; i ++,position += height)
      columns[i] = &data[position];
    memcpy(this->data, rectangleArray.data, sizeof(Type)*this->getWidth()*this->getHeight());
  }
  //------------------------------------------ ASSIGNMENT OPERATOR -------------------------------------------
  template <class Type>
  const RectangleArray<Type>& RectangleArray<Type>::operator=(const RectangleArray<Type>& rectangleArray)
  {
	  this->setWidth(rectangleArray.getWidth());
	  this->setHeight(rectangleArray.getHeight());
	  this->owner = true;
	  this->data = new Type[this->getWidth()*this->getHeight()];
	  columns = new Type*[this->getWidth()];

	  unsigned int i = 0;
	  for (unsigned int position = 0; i < width; i++, position += height)
		  columns[i] = &data[position];
	  memcpy(this->data, rectangleArray.data, sizeof(Type)*this->getWidth()*this->getHeight());
	  return *this;
  }
  //----------------------------------------------- DESTRUCTOR -----------------------------------------------
  template <class Type>
  RectangleArray<Type>::~RectangleArray(void)
  {
     delete [] columns;
     if (this->owner)
       delete [] data;
  }
  //----------------------------------------------- GET WIDTH ------------------------------------------------
  template <class Type>
  unsigned int RectangleArray<Type>::getWidth(void) const
  {
    return this->width;
  }
  //----------------------------------------------- SET WIDTH ------------------------------------------------
  template <class Type>
  void RectangleArray<Type>::setWidth(unsigned int width)
  {
    this->width = width;
  }
  //----------------------------------------------- GET HEIGHT -----------------------------------------------
  template <class Type>
  unsigned int RectangleArray<Type>::getHeight(void) const
  {
    return this->height;
  }
  //----------------------------------------------- SET HEIGHT -----------------------------------------------
  template <class Type>
  void RectangleArray<Type>::setHeight(unsigned int height)
  {
    this->height = height;
  }
  //------------------------------------------------ GET ITEM ------------------------------------------------
  template <class Type>
  Type RectangleArray<Type>::getItem(unsigned int x, unsigned int y)
  {
    return this->getColumn(x)[y];
  }
  //------------------------------------------------ GET ITEM ------------------------------------------------
  template <class Type>
  Type RectangleArray<Type>::getItemSafe(unsigned int x, unsigned int y)
  {
    if (x<0 || y<0 || x>=this->width || y>=this->height)
    {
      return (Type)NULL;
    }
    return this->getColumn(x)[y];
  }
  //------------------------------------------------ SET ITEM ------------------------------------------------
  template <class Type>
  void RectangleArray<Type>::setItem(unsigned int x, unsigned int y, Type* item)
  {
    this->getColumn(x)[y] = item;
  }
  //------------------------------------------------ GET LINE ------------------------------------------------
  template <class Type>
  Type* RectangleArray<Type>::getColumn(unsigned int index)
  {
    return columns[index];
  }
  //------------------------------------------------ GET LINE ------------------------------------------------
  template <class Type>
  const Type* RectangleArray<Type>::getColumn(unsigned int index) const
  {
    return columns[index];
  }
  //------------------------------------------------- RESIZE -------------------------------------------------
  template <class Type>
  void RectangleArray<Type>::resize(unsigned int width, unsigned int height)
  {
    if (this->getWidth() == width &&
        this->getHeight() == height)
      return;

    delete [] this->columns;
    delete [] this->data;  

    this->setWidth(width);
    this->setHeight(height);

    this->data = new Type[this->width * this->height];

    this->columns = new Type*[this->width];
    unsigned int i = 0;
    for (unsigned int position = 0;i < this->width; i ++,position += this->height)
      columns[i] = &data[position];
  }
  //---------------------------------------------- SAVE TO FILE ----------------------------------------------
  template <class Type>
  void RectangleArray<Type>::saveToFile(const std::string& fileName)
  {
	  std::ofstream outputFile(fileName);
	  if (!outputFile)
		  exit(1);

	  for (unsigned int y = 0; y < this->getHeight(); ++y) {
		  for (unsigned int x = 0; x < this->getWidth(); ++x) {
			  outputFile << this->getColumn(x)[y];
		  }
		  outputFile << std::endl;
	  }
	  outputFile.close();
  }
  //------------------------------------------------- SET TO -------------------------------------------------
  template <class Type>
  void RectangleArray<Type>::setTo(const Type& value)
  {
    for (unsigned int i = 0; i < this->getWidth()*this->getHeight(); i++)
      this->data[i] = value;
  }
  //--------------------------------------------- SET BORDER TO ----------------------------------------------
  template <class Type>
  void RectangleArray<Type>::setBorderTo(const Type& value)
  {
    for (unsigned int i = 0; i < this->width; i++)
    {
      this->getColumn(i)[0] = value;
      this->getColumn(i)[this->height - 1] = value;
    }
    for (unsigned int i = 0; i < this->height; i++)
    {
      this->getColumn(0)[i] = value;
      this->getColumn(this->width - 1)[i] = value;
    }    
  }
  //------------------------------------------- SET RECTANGLE TO ---------------------------------------------
  template <class Type>
  void RectangleArray<Type>::setRectangleTo(unsigned int xLeft, unsigned int yTop, unsigned int xRight, unsigned int yBottom, const Type& value) {
	  xLeft = std::max<int>(xLeft,0);
	  yTop = std::max<int>(yTop,0);
	  xRight = std::min<int>(xRight,this->width-1);
	  yBottom = std::min<int>(yBottom,this->height-1);
	  for (unsigned int x = xLeft; x <= xRight; x++) {
		  for (unsigned int y = yTop; y <= yBottom; y++) {
			  this->getColumn(x)[y] = value;
		  }
	  }
  }
  //----------------------------------------------------------------------------------------------------------
}