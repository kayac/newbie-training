Web Application
===============

最も単純な例
------------

以下の `app.psgi` はPSGIを直接利用した Web Application の最も単純な例です。

```perl
use strict;
use warnings;

my $app = sub {
    my $env = shift;

    # your webapp code here

    [ 200, ["Content-Type" => "text/html; charset=utf-8"], ["Hello World"] ];
};

```

起動方法

```
$ cpanm Plack        # Plackのインストール
$ plackup app.psgi
HTTP::Server::PSGI: Accepting connections at http://0:5000/
```

実習(1)
-------

1. ブラウザや `curl` コマンドで http://サーバ名:5000/ にアクセスして、Hello World が表示されることを確認してください
2. サーバ上で `ngrep` コマンドを用いて port 5000 への通信をキャプチャし、どのような HTTP のやり取りがなされているかを確認してください
```
$ ngrep -d eth0 -W byline port 5000
```
3. `t/basic.t` のテストが通ることを確認してください
4. `$env` にどのようなデータが渡ってきているか、Data::Dumper などを使って中身を確認してください

課題(1)
-------

1. `html` ディレクトリに存在するHTMLファイルを利用して、`t/webapp.t` のテストが通る Web Application を `app.psgi` に実装してください


フレームワーク(Amon2)を使った Web Application
------------

インストールとセットアップ
```
$ cpanm Amon2 Amon2::Lite Amon2::DBI
$ PERL5LIB=../lib amon2-setup.pl --flavor=+SimpleFlavor Simple
$ cd Simple
$ rm -rf .git     # setupで自動的にできてしまう .git は削除
$ plackup app.psgi
```

課題(2)
-------

1. 課題(1) で作成した Web Application を Amon2 上で動作するように移植し、t/webapp.t のテストが通るようにしてください
