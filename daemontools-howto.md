damemontools
------------

`server/cookbooks/daemontools` を chef でインストール。

動かない場合はサーバにログインし、以下のコマンドを手動実行で起動してください (あとでrecipeなおします)


```
$ sudo inictl reload-configuration
$ sudo start daemontools
daemontools start/running, process 3163
```

daemontools で起動するアプリケーションの設定
---------------------------------------------

```
$ cd /home/newgrad
$ mkdir -p service/nopaste/log
$ cd service/nopaste
```
`service/nopaste` 以下に `run` というファイルを以下の内容で作成し、実行権限を追加 (`chmod +x run`)

```
#!/bin/sh
set -e

export HOME="/home/newgrad"
export PATH="$HOME/bin:$HOME/.plenv/bin:$PATH"
eval "$(plenv init -)"

# 以下のディレクトリは実際にインストールされた場所に書き換えてください
cd /path/to/newgrad/webapp/NoPaste
exec 2>&1

exec setuidgid newgrad plackup app.psgi
```

`service/nopaste/log` 以下に `run` というファイルを以下の内容で作成し、実行権限を追加 (`chmod +x run`)

```
#!/bin/sh
set -e
exec 2>&1
exec setuidgid newgrad logger -t nopaste
```

手動でまず起動確認

```
$ cd service/nopaste
$ sudo ./run
```

これで無事起動できない場合は `nopaste/run` の内容を見直し

起動できた場合は、daemnotools で起動させます.

```
$ cd /service
$ sudo ln -s /home/newgrad/service/nopaste nopaste
```

ログは `/var/log/messages` に出力されます

daemontools でのプロセス操作
-----------------------------

```
$ sudo svc -d /service/nopaste       # 停止 (down)
$ sudo svc -u /service/nopaste       # 起動 (up)
$ sudo svc -t /service/nopaste       # SIGTERMを送信 (再起動)
$ sudo svc -h /service/nopaste       # SIGHUPを送信 (再起動)
$ sudo svstat /service/nopaste       # プロセス起動状態を表示
```

