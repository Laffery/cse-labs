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

我们自己的性能大概在60w/s.但是多次运行后我们注意到，不同时间段运行会有一定幅度的跳动，如有时候运行native（下文都直接用native和yfs1指代两种不同的运行方式）只有60多而yfs1有20左右，猜想大概是与电脑的运行状态有关，所以要想测试性能我认为最好的方法还是比较native和yfs1的比值，于是将上述两个sh写到同一个文件（test-fxmark.sh）中同时测试。目前我们的性能大概在0.3左右

## bottleneck

### Files Scanning

我们依次从上层到下层来看各个文件

1. fuse.cc

    这一层主要是调用底层的ec，向上提供文件操作方法接口，其本身基本上不存在性能瓶颈，最多能做的就是去掉一些没必要的printf，性能比达到了0.4左右

    ```sh
    # native # ncpu secs works works/sec
    1 5.197879 1152.000000 221.628861
    # yfs1 # ncpu secs works works/sec
    1 5.891832 512.000000 86.899966
    ```

    但是依据其操作的实现却可以给我们一些底层优化的启示，比如：

    - 在getattr中，如果对应的文件不是file，那么yfs中的isfile，isdir都将被调用，可不可以用一个函数来兼具这两个功能呢
    - 还是在getattr中，获取类型之后又调用了getfile等，那何不一次性解决呢

    因此我们将getattr的需要从yfs中获得的信息（is，get）一次性取出，在yfs_client中构造新的函数getAttr，同时构造新的结构体info，将fileinfo，dirinfo，syminfo归一化，包含type,a/c/mtime,size，即全部attr（另一方面为了满足原来isfile之类的需要，提供了一个函数getType返回inode的类型，同时在extent_protocol中新增了T_NONE方便调用）

    ```sh
    #native # ncpu secs works works/sec
    1 5.226676 1152.000000 220.407770
    #yfs1 # ncpu secs works works/sec
    1 5.925878 640.000000 108.000873
    ```

    现在的性能比来到了0.49

2. yfs_client.cc

    同样的我们首先去掉所有不必要的printf

    ```sh
    # native ncpu secs works works/sec
    1 5.215672 1152.000000 220.872785
    # yfs1 # ncpu secs works works/sec
    1 5.816615 896.000000 154.041483
    ```

    有点意外性能比达到了惊人的0.7，反思一下确实是我之前为了方便debug输出了太多啰嗦的废话，于是抱着激动的心情立马前去inode_manager把printf都去掉试试看，发现并没有明显的变化（就不贴出数据了），可能是因为上面两个文件中的输出都是tag表示这一步做了什么，而im中的都是错误处理的输出，本身遇到的次数就不多

    好了下面开始摸查yfs中的每一个操作。下一个是setattr，注意到在ec->get之前我们有做过inum和size的合法性校验，那么我能想到的就是所有的操作都提前校验一下，这样可以减少不必要的底层（ec-im）调用。这个数据我也懒得贴出来了，也许是睡了一晚上做这个的缘故，现在的性能浮动巨大0.5-0.9，我也不太敢信

    郁闷之中我想到了之前买的腾讯云服务器，配置好docker环境后赶紧跑了一下

    ```sh
    # native # ncpu secs works works/sec
    1 5.000182 523648.000000 104725.787981
    # yfs1 # ncpu secs works works/sec
    1 5.000180 39040.000000 7807.718922
    ```

    跑了几次后，舒服了，我们现在有了稳定的性能比，大约在0.07左右，然后回头测试下优化之前的性能，得到如下结果

    ```sh
    # native # ncpu secs works works/sec
    1 5.000649 522752.000000 104536.831119
    # yfs1 # ncpu secs works works/sec
    1 5.038570 24704.000000 4902.978424
    ```

    大概在0.0049左右，证明了我们的优化确实有着明显的效果

3. inode_manager.cc

    inode_manager这一层又仔细阅读了这些代码，把一些输出去掉，然后就是一些对inum的校验，我们新建了一个map using_inodes用来存inode的使用情况，使得inode的有效性只需要根据using_inodes\[inum]来判断，而不需要get_inode

    ```sh
    # native # ncpu secs works works/sec
    1 5.000108 549248.000000 109847.227300
    #yfs1 # ncpu secs works works/sec
    1 5.009340 44032.000000 8789.980317
    ```

    至此，性能达到了0.08左右

到现在为止，lab1的代码全部过完，按照原来代码的逻辑进行的优化也只取得了不到一倍的性能提升，这时候我们很容易会想到使用cache

### Cache

我们注意到yfs_client中很多函数，有一些典型的操作:

- ec->get(ino) 对应im->read_file
- ec->put(ino) 对应im->write_file

就需要在磁盘中进行多次操作，那么我们
