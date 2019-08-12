#ifndef __STRING_H__
#define __STRING_H__

#include <vec.h>

class string {
  char* data;      /*!< The ASCII characters that comprise the string */
  unsigned length; /*!< The number of characters allocated in data */

 public:
  string();

  string(char c);

  string(const char* c);

  string(const string& s);

  ~string();

  unsigned len() const;

  int index(char c) const;

  inline const char* get() const { return data; }




  // split a string on a character c
  vec<string> split(char c, bool include_empty = false);

  char operator[](unsigned j) const;
  char& operator[](unsigned j);
  //@}

  /*!
   *  @brief Sets string value.
   *  @param[in] s A string object.
   *  @return A string reference to *this.
   *  @post string will be equivalent to @a s.
   */
  string& operator=(const string& s);

  /*!
   *  @brief Append to string.
   *  @param[in] s A string object.
   *  @return A string reference to *this.
   *  @post string will equal the concatenation of itself with @a s.
   */
  string& operator+=(const string& s);

  //@{
  /*!
   *  @brief string concatenation (addition).
   *  @param[in] lhs The left-hand operand string or string convertable.
   *  @param[in] rhs The right-hand operand string or string convertable.
   *  @return Copy of a string object.
   *  @post The string will be the concatenation of @a lhs and @a rhs.
   */
  friend string operator+(const string& lhs, const string& rhs);
  friend string operator+(const string& lhs, char rhs);
  friend string operator+(const string& lhs, const char* rhs);
  friend string operator+(char lhs, const string& rhs);
  friend string operator+(const char* lhs, const string& rhs);
  //@}

  //@{
  /*!
   *  @brief string equality
   *  @param[in] lhs The left-hand operand string or string convertable.
   *  @param[in] rhs The right-hand operand string or string convertable.
   *  @return True, if @a lhs and @a rhs have the same length, and each
   *          character in their string data are identical in both value
   *          and index.
   */
  friend bool operator==(const string& lhs, const string& rhs);
  friend bool operator==(const string& lhs, char rhs);
  friend bool operator==(const string& lhs, const char* rhs);
  friend bool operator==(char lhs, const string& rhs);
  friend bool operator==(const char* lhs, const string& rhs);
  //@}

  //@{
  /*!
   *  @brief string inequality: Greater-than.
   *  @param[in] lhs The left-hand operand string or string convertable.
   *  @param[in] rhs The right-hand operand string or string convertable.
   *  @return True, if @a lhs is in dictionary order (Capitals-first) to
   *          @a rhs when comparing alphabetical characters or @a lhs is
   *          greater in ASCII value to @a rhs, in corresponding string
   *          data indices.
   */
  friend bool operator>(const string& lhs, const string& rhs);
  friend bool operator>(const string& lhs, char rhs);
  friend bool operator>(const string& lhs, const char* rhs);
  friend bool operator>(char lhs, const string& rhs);
  friend bool operator>(const char* lhs, const string& rhs);
  //@}

  //@{
  /*!
   *  @brief string non-equality
   *  @param[in] lhs The left-hand operand string or string convertable.
   *  @param[in] rhs The right-hand operand string or string convertable.
   *  @return True, if @a lhs is not equal to @a rhs.
   *  @see string::operator==
   */
  friend bool operator!=(const string& lhs, const string& rhs);
  friend bool operator!=(const string& lhs, char rhs);
  friend bool operator!=(const string& lhs, const char* rhs);
  friend bool operator!=(char lhs, const string& rhs);
  friend bool operator!=(const char* lhs, const string& rhs);
  //@}

  //@{
  /*!
   *  @brief string inequality: Less-than.
   *  @param[in] lhs The left-hand operand string or string convertable.
   *  @param[in] rhs The right-hand operand string or string convertable.
   *  @return True, if @a lhs is neither equal to, nor greater-than @a rhs.
   *  @see string::operator==,string::operator>
   */
  friend bool operator<(const string& lhs, const string& rhs);
  friend bool operator<(const string& lhs, char rhs);
  friend bool operator<(const string& lhs, const char* rhs);
  friend bool operator<(char lhs, const string& rhs);
  friend bool operator<(const char* lhs, const string& rhs);
  //@}

  //@{
  /*!
   *  @brief string inequality: Less-than or equal
   *  @param[in] lhs The left-hand operand string or string convertable.
   *  @param[in] rhs The right-hand operand string or string convertable.
   *  @return True, if @a lhs is not greater-than @a rhs.
   *  @see string::operator>
   */
  friend bool operator<=(const string& lhs, const string& rhs);
  friend bool operator<=(const string& lhs, char rhs);
  friend bool operator<=(const string& lhs, const char* rhs);
  friend bool operator<=(char lhs, const string& rhs);
  friend bool operator<=(const char* lhs, const string& rhs);
  //@}

  //@{
  /*!
   *  @brief string inequality: Greater-than or equal
   *  @param[in] lhs The left-hand operand string or string convertable.
   *  @param[in] rhs The right-hand operand string or string convertable.
   *  @return True, if @a lhs is greater-than or equal to @a rhs.
   *  @see string::operator>,string::operator==
   */
  friend bool operator>=(const string& lhs, const string& rhs);
  friend bool operator>=(const string& lhs, char rhs);
  friend bool operator>=(const string& lhs, const char* rhs);
  friend bool operator>=(char lhs, const string& rhs);
  friend bool operator>=(const char* lhs, const string& rhs);
  //@}
};

#endif
