# これはなに?

この実習では、Linux サーバの基本操作と環境設定を身につけることを目的にしています。

serverspec http://serverspec.org/ でテストすることで、目的の構成になっているかどうかを確認しつつ構築していきます。

## 準備

### 手元のマシンで

* rbenv で ruby-1.9.3-p394 (もしくは1.9.3系の最新) をインストールし、使える状態にします
 * Linux の場合、openssl-devel パッケージを事前にインストールしておくこと
* `gem install bundler` を実行し、bundler をインストールします
* `bundle install` を実行すると、必要な gem が `vendor/` ディレクトリ以下にインストールされます
* `spec/example` ディレクトリに対して、対象サーバのホスト名 (e.g. `test.newgrad.kayac`) のシンボリックリンクを作成します
* `nodes/example.json` をコピーし、対象サーバのホスト名.json (e.g. `test.newgrad.kayac.json`) を作成します
```
$ gem install bundler
$ cd newgrad/server/
$ bundle install
$ ln -s example spec/test.newgrad.kayac
$ cp nodes/example.json nodes/test.newgrad.kayac.json
```

* `~/.ssh/config` で以下の設定をします

```
Host test.newgrad.kayac
  User newgrad
  StrictHostKeyChecking no
  UserKnownHostsFile /dev/null
```

### テスト対象サーバについて

* `newgrad` ユーザを作成し、パスワードなしで sudo 実行可能にしておきます
* `/home/newgrad/.ssh/authorized_keys` に自分の公開鍵を設定します

(この部分は、研修で使用するサーバでは事前に設定済みです)

## serverspecの実行

`test.newgrad.kayac` の部分は各自が使用するサーバのホスト名に読み替えてください

```
$ bundle exec rake host=test.newgrad.kayac
ruby -S rspec spec/admin.newgrad.kayac/server_spec.rb
.....................................

Finished in 4.35 seconds
37 examples, 0 failures
```

すべてのテストをpassすると、0 failuersで終了します。初期状態では当然ながら通りません。

## 実習(1)

* 各自のサーバについて、`spec/localhost/server_spec.rb` に定義されたテストがすべて通るように、手動で設定してください
* 設定を行う際には、行った操作のメモを取ってください
  * 参照したページがある場合はその URL も記録してください


## knife-solo による chef-solo の実行

`test.newgrad.kayac` の部分は各自が使用するサーバのホスト名に読み替えてください

まず対象サーバに chef をインストールするため、`knife solo prepare` を実行します。
```
$ bundle exec knife solo prepare test.newgrad.kayac
Downloading Chef  for el...
Installing Chef
Preparing...                ########################################### [100%]
   1:chef                   ########################################### [100%]
Thank you for installing Chef!
```

`nodes/example.json` を `nodes/対象サーバ名.json` に copy します。

```
$ cp nodes/example.json nodes/test.newgrad.kayac.json
$ cat nodes/test.newgrad.kayac.json
{
  "run_list": ["example"],
  "example": {
    "text": "World"
  }
}
```

`nodes/test.newgrad.kayac.json` では `example` の recipe を実行するように定義されているので、実行してみます。

```
$ bundle exec knife solo cook test.newgrad.kayac
Checking Chef version...
Starting Chef Client, version 11.4.0
Compiling Cookbooks...
Converging 1 resources
Recipe: example::default
  * template[/tmp/example.file] action create
    - create template[/tmp/example.file]
        --- /tmp/chef-tempfile20130404-19774-10vzuui	2013-04-04 01:01:29.127813283 +0900
        +++ /tmp/chef-rendered-template20130404-19774-1tlmske	2013-04-04 01:01:29.127813283 +0900
        @@ -0,0 +1 @@
        +Hello World on test.newgrad.kayac

Chef Client finished, 1 resources updated
```

recipeが実行され、/tmp/example.file が作成されました。

serverspecで example_spec のみを実行し、通ることを確認しましょう。

```
$ bundle exec rake host=test.newgrad.kayac example_spec
ruby -S rspec spec/test.newgrad.kayac/example_spec.rb
....

Finished in 0.4401 seconds
4 examples, 0 failures
```

### 独自 cookbook の作成

たとえば ntp の cookbook を作成する場合、以下のように `knife cookbook create` を実行することで、`cookbooks/ntp` が作成されます。実行する場合は、`nodes/*.json` の run_list に追加します。

```
$ bundle exec knife cookbook create ntp
** Creating cookbook ntp
** Creating README for cookbook: ntp
** Creating CHANGELOG for cookbook: ntp
** Creating metadata for cookbook: ntp
```

```
{
  "run_list": ["example", "ntp"],
  "example": {
    "text": "World"
  }
}
```

## 実習(2)

* 各自のサーバについて、`spec/localhost/server_spec.rb` に定義されたテストがすべて通るように、Chefを使用して設定してください。
 * 作成した cookbook はリポジトリにコミットしてください
 
