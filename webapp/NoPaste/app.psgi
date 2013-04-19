package NoPaste;

use strict;
use warnings;
use utf8;
use File::Spec;
use File::Basename;
use lib File::Spec->catdir(dirname(__FILE__), 'extlib', 'lib', 'perl5');
use lib File::Spec->catdir(dirname(__FILE__), 'lib');
use Amon2::Lite;
use Plack::Session::Store::File;
use FormValidator::Lite;
use Crypt::SaltedHash;
use Log::Minimal;

our $VERSION = '0.01';
our $DSN = [
    'dbi:mysql:database=test;host=127.0.0.1;port=3306',
    'root',
    '',
];

# put your configuration here
sub load_config {
    my $c = shift;
    +{
        DBI => $DSN,
        recent_posts_limit => 100,
    }
}

sub recent_posts {
    my ($c) = @_;

    my @recent_posts = (
        {
            id       => 1,
            username => "Dummy user",
            stars    => 10,
            headline => "Dummy content",
        },
    );
    # -------------------------------------------------
    # sidebar に表示する最新post一覧を生成してください
    # 件数は最大 $c->config->{recent_posts_limit}
    # -------------------------------------------------

    \@recent_posts;
}

get '/' => sub {
    my $c = shift;

    return $c->render('index.tt', {
        recent_posts => $c->recent_posts(),
    });
};

post '/post' => sub {
    my $c = shift;

    my $validator = FormValidator::Lite->new($c->req);
    my $result    = $validator->check(
        content => [ qw/ NOT_NULL / ],
    );
    my $username  = $c->session->get("username");
    if ( !defined $username ) {
        return $c->create_response( 400, [], [ "invalid request" ] );
    }

    if ( $validator->has_error ) {
        $c->fillin_form($c->req);
        return $c->render('index.tt', {
            errors       => $validator->errors,
            recent_posts => $c->recent_posts(),
        });
    }

    my $user = $c->dbh->selectrow_arrayref(
        "SELECT id FROM users WHERE username=?",
        {}, $username,
    );
    my $user_id = $user->[0];
    my $content = $c->req->param("content");
    $c->dbh->do(
        "INSERT INTO posts (user_id, content) VALUES (?, ?)",
        {},
        $user_id, $content,
    );
    my $post_id = $c->dbh->{'mysql_insertid'};
    return $c->redirect("/post/$post_id");
};

get '/post/:id' => sub {
    my ($c, $args) = @_;

    my $post_id = $args->{id};

    # ------------------------------------
    # postを表示する機能を実装してください
    # ------------------------------------

    $c->render('post.tt', {
        post => {
            id         => $args->{id},
            content    => "Dummy content\nfoo\nbar",
            username   => "Dummy user",
            stars      => 10,
            created_at => "2013-04-10 15:26:40",
        },
        recent_posts => $c->recent_posts(),
    });
};

post '/star/:id' => sub {
    my ($c, $args) = @_;

    my $username = $c->session->get("username");
    unless ($username) {
        return $c->create_response( 400, [], [ "invalid request" ] );
    }

    my $post_id = $args->{id};

    my $post = $c->dbh->selectrow_arrayref(
        "SELECT id, user_id, content, created_at FROM posts WHERE id=?",
        {},
        $post_id,
    );
    return $c->res_404 unless $post;

    my $user = $c->dbh->selectrow_arrayref(
        "SELECT id FROM users WHERE username=?",
        {}, $username,
    );
    my $user_id = $user->[0];

    $c->dbh->do(
        "INSERT INTO stars (post_id, user_id) VALUES (?, ?)",
        {},
        $post_id, $user_id,
    );
    return $c->redirect("/post/$post_id");
};

get '/signin' => sub {
    my $c = shift;
    return $c->render('signin.tt');
};

get '/signout' => sub {
    my $c = shift;
    $c->session->expire;
    return $c->redirect('/');
};

post '/signin' => sub {
    my $c = shift;
    my $username  = $c->req->param("username");
    my $password  = $c->req->param("password");

    # -----------------------------
    # ログイン処理を入れてください
    # -----------------------------
    my $success = 0;
    if ( $success ) {
        # ログインに成功した場合
        infof "signin success username: %s", $username;
        $c->session->set( username => $username );
        # session fixation 対策で session id を再生成する
        $c->req->session_options->{change_id}++;
        return $c->redirect("/");
    }

    # ログイン失敗した場合
    my $validator = FormValidator::Lite->new($c->req);
    $validator->set_error( login => "FAILED" );

    warnf "signin failed username: %s", $username;
    $c->fillin_form($c->req);
    return $c->render('signin.tt', { errors => $validator->errors });
};

get '/signup' => sub {
    my $c = shift;
    return $c->render('signup.tt');
};

post '/signup' => sub {
    my $c = shift;

    my $username = $c->req->param("username");
    my $password;

    # see http://search.cpan.org/dist/FormValidator-Lite/lib/FormValidator/Lite.pm
    # and http://search.cpan.org/dist/FormValidator-Lite/lib/FormValidator/Lite/Constraint/Default.pm
    my $validator = FormValidator::Lite->new($c->req);

    # --------------------------------------
    # 入力の validate 処理を入れてください
    # username: 必須 2文字以上20文字以下 半角アルファベットと数字のみ
    # password: 必須 2文字以上20文字以下 ASCII のみ
    # --------------------------------------

    # validationでエラーが起きたらフォームを再表示
    if ( $validator->has_error ) {
        debugf "signup validate error: %s", ddf $validator->errors;
        $c->fillin_form($c->req);
        return $c->render('signup.tt', { errors => $validator->errors });
    }

    # validationを通ったのでユーザを作成
    $c->dbh->do(
        "INSERT INTO users (username, password) VALUES (?, ?)",
        {},
        $username, $password,
    );

    infof "signup success: username: %s", $username;
    # セッションにユーザ名を保存して / にリダイレクト
    $c->session->set( username => $username );
    return $c->redirect('/');
};


# load plugins
__PACKAGE__->load_plugin('Web::CSRFDefender');
__PACKAGE__->load_plugin('DBI');
__PACKAGE__->load_plugin('Web::FillInFormLite');
# __PACKAGE__->load_plugin('Web::JSON');

__PACKAGE__->enable_session(
    store => Plack::Session::Store::File->new( dir => "./tmp" ),
);

__PACKAGE__->to_app(handle_static => 1);

__DATA__

