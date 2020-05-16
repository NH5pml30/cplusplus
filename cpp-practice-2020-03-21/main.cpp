/* Kholiavin Nikolai, M3138 */

#include <cstdlib>
#include <iostream>
#include <cstring>

void prefix(const char *s, size_t len, size_t *p)
{
  p[0] = 0;
  for (size_t k = 0, i = 1; i < len; i++)
  {
    while (k > 0 && s[k] != s[i])
      k = p[k - 1];
    if (s[k] == s[i])
      k++;
    p[i] = k;
  }
}

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    std::cerr << "usage: " << argv[0] << " <substring> <filename>\n";
    return EXIT_FAILURE;
  }

  FILE *f = fopen(argv[2], "r");
  if (!f)
  {
    perror("fopen failed");
    return EXIT_FAILURE;
  }

  size_t
    len = strlen(argv[1]),
    *p = (size_t *)malloc(sizeof(size_t) * len);
  if (!p)
  {
    perror("malloc failed");
    fclose(f);
    return EXIT_FAILURE;
  }
  prefix(argv[1], len, p);
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
        free(p);
        fclose(f);
        return EXIT_FAILURE;
      }

      break;
    }

    for (size_t i = 0; i < bytes_read; i++)
    {
      while (k > 0 && argv[1][k] != buffer[i])
        k = p[k - 1];
      if (buffer[i] == argv[1][k])
        k++;
      if (k == len)
      {
        result = true;
        break;
      }
    }
  } while (!result);

  free(p);
  fclose(f);

  std::cout << std::boolalpha << result << std::endl;
  return EXIT_SUCCESS;
}
