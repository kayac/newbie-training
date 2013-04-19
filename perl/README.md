Perl
====

はてなさんの [Perlによるオブジェクト指向プログラミング](https://github.com/hatena/Hatena-Textbook/blob/master/oop-for-perl.md) に沿って適宜進めます。

課題1
=====

`t/sorter.t` を各自のディレクトリにcopyしてテストが通るように `lib/Sorter.pm` を実装してください。

テスト実行
```
$ tree
.
├── lib
│   └── Sorter.pm
└── t
    └── sort.t

$ prove -lvr t
t/sort.t ..
    ok 1 - The object isa Sorter
    1..1
ok 1 - init
    ok 1
    ok 2
    ok 3
    ok 4
    1..4
ok 2 - values
    ok 1
    ok 2
    ok 3
    ok 4
    ok 5
    ok 6
    ok 7
    ok 8
    ok 9
    ok 10
    ok 11
    1..11
ok 3 - sort
1..3
ok
All tests successful.
Files=1, Tests=3,  0 wallclock secs ( 0.03 usr  0.01 sys +  0.06 cusr  0.01 csys =  0.11 CPU)
Result: PASS
```

課題2
=====

下記のインターフェースを満たす `My::List` とそのテストを作成してください。

```perl
my $list = My::List->new;

$list->append("Hello");
$list->append("World");
$list->append(2008);

my $iter = $list->iterator;
while ($iter->has_next) {
    print $iter->next->value;
}
```
