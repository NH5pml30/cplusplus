import random
import os

if not os.path.isdir("./sub_tests"):
  os.mkdir("sub_tests")
if not os.path.isdir("./mult_tests"):
  os.mkdir("mult_tests")

maxmax = 1 << (64 * 128)

for i in range(100):
  qwords = random.randint(1, 128)
  max = 1 << (64 * qwords)
  a = random.getrandbits(64 * qwords)
  b = random.getrandbits(64 * qwords)
  res = a - b
  if res < 0:
    res += maxmax

  f = open("sub_tests/test" + str(i) + ".txt", "w")
  f.write(str(a) + '\n' + str(b) + '\n')
  f.close()
  g = open("sub_tests/test" + str(i) + ".gold", "w")
  g.write(str(res) + '\n')
  g.close()

for i in range(100):
  qwords = random.randint(1, 128)
  max = 1 << (64 * qwords)
  a = random.getrandbits(64 * qwords)
  b = random.getrandbits(64 * qwords)
  res = a * b
  res -= res / maxmax * maxmax

  f = open("mult_tests/test" + str(i) + ".txt", "w")
  f.write(str(a) + '\n' + str(b) + '\n')
  f.close()
  g = open("mult_tests/test" + str(i) + ".gold", "w")
  g.write(str(res) + '\n')
  g.close()

