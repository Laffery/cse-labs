# CSE-2020-fall lab1++ TuneYFS

## Getting started

首先拉取[gitlab](http://ipads.se.sjtu.edu:1312/lab)上新分支的文件，然后将原来[lab1](https://github.com/Laffery/cselab1)中的代码写进相应的位置

> make
>
> ./grade.sh

## fxmark

首先在fxmark目录下make，然后建立一个工作目录

> mkdir native

然后运行（写在test-fxmark-native.sh中）

> ./fxmark/bin/fxmark --type=YFS --root=./native --ncore=1 --duration=5

测得几次标准的性能数据

---
|\\| ncpu | secs | works | works/sec |
|-|-|-|-|-|
| 1 | 1 | 5.034763 | 1024.000000 | 203.385939 |
| 2 | 1 | 5.508138 | 1152.000000 | 209.145087 |
| 3 | 1 | 5.525076 | 1152.000000 | 208.503919 |
| 4 | 1 | 5.501405 | 1152.000000 | 209.401053 |
| 5 | 1 | 5.646466 | 1152.000000 | 204.021418 |
---

可以看到benchmark的性能大概在205w/s

然后测试我们自己的性能（写在test-fxmark-yfs1.sh中）

> ./start.sh
>
> ./fxmark/bin/fxmark --type=YFS --root=./native --ncore=1 --duration=5
>
> ./stop.sh

---
|\\| ncpu | secs | works | works/sec |
|-|-|-|-|-|
| 1 | 1 | 6.398255 | 384.000000 | 60.016364 |
| 2 | 1 | 6.536976 | 384.000000 | 58.742758 |
| 3 | 1 | 6.252829 | 384.000000 | 61.412202 |
| 4 | 1 | 6.531211 | 384.000000 | 58.794609 |
| 5 | 1 | 6.354965 | 384.000000 | 60.425195 |
---

我们自己的性能大概在60w/s

## bottleneck
