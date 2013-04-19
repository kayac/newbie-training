Amon2 を使用した NoPaste アプリケーションの実装実習
------------------------------------------------

課題
-----
アプリケーションのリファレンス実装から所々省いたものが `app.psgi` として配置されています。

1. `app.psgi` の内部を参考に、`t/webapp.t` のテストが通る状態まで実装してください。
2. テストは signup, signin, toppage については実装してあるので、残りの `get "/post/:id"`, `get "/star/:id"`, `post "/post"` についてテストを記述してください


準備と実行
-----

```
$ cpanm --installdeps .
$ mysql -uroot test < sql/nopaste.sql
$ plackup -r app.psgi
```

テスト実行
----------

```
$ perl -MTest::Pretty t/webapp.t
```

ISUCON計測用初期データ投入
---------------------------

```
$ gzip -dc sql/initial.sql.gz | mysql -uroot test
```

本番計測前に一旦初期データにリセットします。
Schema変更を行う場合は、この後に実行してください。
