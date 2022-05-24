

#include <iostream>
#include <cstring>
#include <string>
#include <string.h>

template<size_t SOURCE_SIZE>
void my_safe_copy(char (& dest)[SOURCE_SIZE], const char * source)
{
  strncpy_s(dest, SOURCE_SIZE, source, SOURCE_SIZE - 1);
}

template<size_t SOURCE_SIZE>
void my_safe_copy(char (& dest)[SOURCE_SIZE], const std::string & source)
{
  strncpy_s(dest, SOURCE_SIZE, source.c_str(), SOURCE_SIZE - 1);
}

int main(int, char **)
{
  char dest[4] = { 0 };
  const char * src = "hello";
  my_safe_copy(dest, src);
  std::cout << "Dest=[" << dest << "]" << std::endl;
  return 0;    
}
