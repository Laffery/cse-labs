# CSE-2020-fall lab3 YourDataBase

## Getting started

git合并后运行

> ./grade_lab2.sh

## Part1 NONE

在ydb_server.cc里面差不多只用调ec就行了，主要问题是这个测试中间会把ydb_server重启，如果将key保存在这里面重启的时候将get不到任何数据

所以选择使用hash，这里用的简单的hash，将字符串转化成一个uint32_t即inum，由于重启的原因就不缓存了

于是

```sh
Start testing part1 : no transaction

start no transaction test-lab3-durability
[^_^] Pass test-lab3-durability
Passed part1

Finish testing part1 : no transaction

----------------------------------------
```
