/* Kholiavin Nikolai, M3138 */

#include <cstdlib>
#include <iostream>
#include <cstring>
#include <vector>
#include <string>

std::vector<size_t> prefix(const std::string &s)
{
  std::vector<size_t> p(s.length());
  p[0] = 0;
  for (size_t k = 0, i = 1; i < s.length(); i++)
  {
    while (k > 0 && s[k] != s[i])
      k = p[k - 1];
    if (s[k] == s[i])
      k++;
    p[i] = k;
  }
  return p;
}

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    std::cerr << "usage: " << argv[0] << " <substring> <filename>\n";
    return EXIT_FAILURE;
  }

  std::string s = argv[1];
  FILE *f = fopen(argv[2], "r");
  if (!f)
  {
    perror("fopen failed");
    return EXIT_FAILURE;
  }

  auto p = prefix(s);
  bool result = false;
  size_t k = 0;

  do
  {
    char buffer[8192];
    size_t bytes_read = fread(buffer, sizeof(char), std::size(buffer), f);

    if (bytes_read == 0)
    {
      if (ferror(f))
      {
        perror("fread failed");
        fclose(f);
        return EXIT_FAILURE;
      }

      break;
    }

    for (size_t i = 0; i < bytes_read; i++)
    {
      while (k > 0 && s[k] != buffer[i])
        k = p[k - 1];
      if (buffer[i] == s[k])
        k++;
      if (k == s.length())
      {
        result = true;
        break;
      }
    }
  } while (!result);

  fclose(f);

  std::cout << std::boolalpha << result << std::endl;
  return EXIT_SUCCESS;
}
