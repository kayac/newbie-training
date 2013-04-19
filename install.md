
インストール作業
=================

* 2013/4/4でやる作業

----

Windowsの人
=============

* めんどくさいです
* Macの人と同等の環境を揃えるために以下の作業が必要です
  1. VirtualBoxをインストール
  2. CentOS 6.4 (x64)をインストール
  3. TeraTermかPuTTYあたりでLinuxにSSHログインできるようにする
  4. 公開鍵でLinuxにログインできるように設定
* yumで以下のパッケージを最低限いれる
  * groupinstall 'Development Tools'

Virtual Boxに割り当てるメモリは全体の1/4くらいを目安に。
コアは1個か2個で十分。

公開鍵でログインする設定はこの先何度も出てくるので身につけておくこと。特にWindowsの場合はPuTTYのkeygenの吐く公開鍵はOpenSSHで直接使えないので変換が必要ということは覚えておくこと(1回やってそれを取っておけばOK)。

cygwin? 趣味レベルならそれでもなんとかなるかもしれないけど、昔と違って今はCPUの仮想化支援機能が充実してるからVMを建てた方がよい。X11入れなければほとんどCPU食わない。


Macの人
============

* Xcodeをインストール
  * Commandline Toolsもちゃんと入れること
* homebrewをインストール

Xcodeを入れたくなければCommandline Toolsだけ入れるのでもよい。
https://developer.apple.com/downloads/index.action から単体でダウンロードできる(要Apple ID)

----


LL言語の開発環境
======================

必須
----

* perl環境 (plenv)
  * plenv install 5.16.3 -DDEBUGGING=-g
  * その後 cpanm を入れる(モジュール管理)
* ruby環境 (rbenv)
  * rbenv install 1.9.3-p392
  * rbenvならgemは勝手に入ります

オプション
-----------

* python (自前でビルド)
* node.js (nodebrew)


なぜplenv/rbenvか
=============================

* root権がなくてもモジュールを追加インストールしたい
  * システム標準では /usr/bin などのroot権が必要なところにバイナリがある
* 複数バージョンを切り替えたい
  * プロジェクトによって使うバージョンが異なる
* こんな感じでつかう
  * plenv global 5.16.3
  * cd ~/my/project/; plenv local 5.14.3


その他の実行環境
================

* perl
  * perlbrew
* ruby
  * rvm
* node
  * nvm
  * nave
* ○vmか○brewか○envっぽい

----

インフラ系
=============================

Daemonize
---------
* daemontools (or superviserd)

ログ取り
--------
* fluentd

サーバセットアップ
------------------
* capistrano
* chef
* knife

Webサーバ
----------
* nginx
* apache or lighttpd

データストア系
===================

RDB
-----
* mysqld

カヤックではMySQL以外のRDBを使うことはほぼありません。

その他のRDBといえばSQL Server(mssql)とかOracleとかPostgresとかsqlite3とかがある。


KVS (Key Value Store)
----------------------

* memcached
* KyotoTycoon
* Redis

ドキュメント指向
----------------
* MongoDB

JSONを突っ込めるDB! JSONの各キーに対してINDEXを貼ることができるのでJSONを検索するのに便利!

----

開発を便利にするツール
======================

ファイル内検索
--------------
* ag or ack

これまではackというPerlで書かれたツールが定番でしたが、最近Pythonで書かれたThe Silver Searcherというツールがでてきてこちらのほうが高速と言われています。なんといってもackと打つよりagと打つ方が33%も高速化。(参考: http://blog.glidenote.com/blog/2013/02/28/the-silver-searcher-better-than-ack/ )


ログインセッションの持続
--------------------------
* tmux or screen

これはどっち使ってもいいですが少なくともどちらも使わないというのは選択肢としてないかなーという感じ。サーバにSSHで入って作業している最中、たとえば時間のかかるコマンドを実行してる最中に手元のネットワークが切れたりすると悲惨というより事故につながることがあります。これを防ぐためにサーバの上でセッションが持続するtmux/screenは必ず使うのを身につけてください。

tmux/screenのどっちがいいかというのは人それぞれですが、tmuxのほうがスクリーン分割の自由度は高いです。

バージョン管理
----------------
* git (or svn)
* git-flow

バージョン管理は今更svnつかう必要ないよねという感じです。もしかすると新卒の人たちもgitネイティブ世代(svn使ったことない世代)の人が多いのではないでしょうか。ということでgitを使うようにしてください。

その上で、git-flowというgitのユースケースを知っておくのは大事です。(参考: http://www.oreilly.co.jp/community/blog/2011/11/branch-model-with-git-flow.html )


ライフチェンジング系(?)
------------
* mosh

moshはMobile Shellの名前の通り、電車移動中とかで接続が切れてもストレスにならないようなリモートシェルです。具体的にはコネクション指向のTCPではなく、データグラムを送りつけるだけのUDPで実装されています。mosh内部でうまく届かなかったときの再送処理を自前で実装しているため、内部処理は(たぶん)複雑ですが、その分通信切断したときも非常に柔軟に振る舞います。

