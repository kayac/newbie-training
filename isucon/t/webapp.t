use strict;
use warnings;
use Plack::Test;
use Test::More;
use HTTP::Request::Common;
use List::Util qw/ shuffle /;
use Path::Class qw/ file /;
use t::Util;

my ($app, $mysqld) = setup;
my @words = shuffle ( map { chomp; $_ } file("/usr/share/dict/words")->slurp );

subtest_psgi "top", $app, sub {
    my $cb = shift;

    my $res = request $cb, GET "http://localhost/";

    is $res->code, 200;
    is $res->header("Content-Type") => "text/html; charset=UTF-8";
    my $nodes = findnodes($res->content);
    my $title = $nodes->("title")->[0];
    is $title->as_trimmed_text, "NoPaste";

    my $link = $nodes->("div.hero-unit p a")->[0];
    ok $link;
    is $link->attr("href") => "/signin";
    is $link->as_trimmed_text, "Please sign in";
};

subtest_psgi "signup ok", $app, sub {
    my $cb  = shift;

    my $res = request $cb, GET "http://localhost/signup";

    is $res->code, 200;
    is $res->header("Content-Type") => "text/html; charset=UTF-8";

    my $nodes = findnodes($res->content);
    my $h2    = $nodes->("h2.form-signin-heading")->[0];
    is $h2->as_trimmed_text, "Sign up now!";

    my $form = $nodes->("form.form-signin")->[0];
    is $form->attr("method"), "post";
    is $form->attr("action"), "/signup";
    ok $nodes->("form.form-signin input[name='username']")->[0];
    ok $nodes->("form.form-signin input[name='password']")->[0];
    ok $nodes->("form.form-signin input[name='password_confirm']")->[0];
    my $hidden = $nodes->("form.form-signin input[name='csrf_token']")->[0];
    ok $hidden;
    ok $hidden->attr("value");

    my $username = "test$$";
    my $password = "pass$$";
    $res = request(
        $cb,
        POST "http://localhost/signup",
        [
            username         => $username,
            password         => $password,
            password_confirm => $password,
            csrf_token       => $hidden->attr("value"),
        ],
    );
    is $res->code => 302;
    is $res->header("Location") => "http://localhost/";

    $res = request $cb, GET $res->header("Location");
    is $res->code => 200;
    $nodes = findnodes($res->content);
    my $p = $nodes->("p.navbar-text")->[0];
    ok $p;
    like $p->as_trimmed_text, qr/Logged in as \Q$username\E/;
};

subtest_psgi "signup csrf error", $app, sub {
    my $cb  = shift;

    my $res = request $cb, GET "http://localhost/signup";
    is $res->code, 200;
    my $nodes = findnodes($res->content);
    my $hidden = $nodes->("form.form-signin input[name='csrf_token']")->[0];
    ok $hidden;
    ok $hidden->attr("value");

    my $username = "test$$";
    my $password = "pass$$";
    $res = request(
        $cb,
        POST "http://localhost/signup",
        [
            username         => $username,
            password         => $password,
            password_confirm => $password,
        ],
    );
    is $res->code => 403;
};

subtest_psgi "signup validation error", $app, sub {
    my $cb  = shift;

    my $res = request $cb, GET "http://localhost/signup";
    is $res->code, 200;
    my $nodes = findnodes($res->content);
    my $hidden = $nodes->("form.form-signin input[name='csrf_token']")->[0];
    ok $hidden;
    ok $hidden->attr("value");

    my $username = "test$$";
    my $password = "pass$$";
    $res = request(
        $cb,
        POST "http://localhost/signup",
        [
            username         => $username,
            password         => $password,
            password_confirm => $password,
            csrf_token       => $hidden->attr("value"),
        ],
    );
    is $res->code => 200;
    $nodes = findnodes($res->content);
    my $errors = $nodes->("div.error span.help-inline");
    is $errors->[0]->as_trimmed_text, "Already exists";

    $username = "xxxx$$";
    $password = "pass$$";
    $res = request(
        $cb,
        POST "http://localhost/signup",
        [
            username         => $username,
            password         => $password,
            password_confirm => "${password}xxx",
            csrf_token       => $hidden->attr("value"),
        ],
    );
    is $res->code => 200;
    $nodes = findnodes($res->content);
    $errors = $nodes->("div.error span.help-inline");
    is $errors->[0]->as_trimmed_text, "Confirm mismatch";
};

subtest_psgi "signout", $app, sub {
    my $cb  = shift;

    my $res = request $cb, GET "http://localhost/signout";
    is $res->code, 302;
    is $res->header("Location") => "http://localhost/";

    $res = request $cb, GET $res->header("Location");
    is $res->code => 200;
    my $nodes = findnodes($res->content);
    my $p = $nodes->("p.navbar-text")->[0];
    ok $p;
    like $p->as_trimmed_text, qr/Sign in/;
};

subtest_psgi "signin ok", $app, sub {
    my $cb  = shift;

    my $res = request $cb, GET "http://localhost/signin";
    is $res->code, 200;

    my $nodes = findnodes($res->content);
    my $form = $nodes->("form.form-signin")->[0];
    is $form->attr("method"), "post";
    is $form->attr("action"), "/signin";
    ok $nodes->("form.form-signin input[name='username']")->[0];
    ok $nodes->("form.form-signin input[name='password']")->[0];
    my $hidden = $nodes->("form.form-signin input[name='csrf_token']")->[0];
    ok $hidden;
    ok $hidden->attr("value");

    my $username = "test$$";
    my $password = "pass$$";
    $res = request(
        $cb,
        POST "http://localhost/signin",
        [
            username         => $username,
            password         => $password,
            csrf_token       => $hidden->attr("value"),
        ],
    );
    is $res->code, 302;
    ok $res->header("Location"), "http://localhost/";

    $res = request $cb, GET $res->header("Location");
    is $res->code => 200;
    $nodes = findnodes($res->content);
    my $p = $nodes->("p.navbar-text")->[0];
    ok $p;
    like $p->as_trimmed_text, qr/Logged in as \Q$username\E/;
};

subtest_psgi "signin failed", $app, sub {
    my $cb  = shift;

    my $res = request $cb, GET "http://localhost/signin";
    is $res->code, 200;

    my $nodes  = findnodes($res->content);
    my $hidden = $nodes->("form.form-signin input[name='csrf_token']")->[0];
    ok $hidden;
    ok $hidden->attr("value");

    my $username = "test$$";
    $res = request(
        $cb,
        POST "http://localhost/signin",
        [
            username   => $username,
            password   => "xxxx",
            csrf_token => $hidden->attr("value"),
        ],
    );
    is $res->code, 200;
    $nodes = findnodes($res->content);
    my $errors = $nodes->("div.error span.help-inline");
    is $errors->[0]->as_trimmed_text, "FAILED";
};

subtest_psgi "top_post_star", $app, sub {
    my $cb = shift;

    my $res = request $cb, GET "http://localhost/";

    my $nodes = findnodes($res->content);
    my $form = $nodes->("div.hero-unit form")->[0];
    ok $form;
    is $form->attr("action") => "/post";
    is $form->attr("method") => "post";
    my $textarea = $nodes->("div.hero-unit form textarea[name='content']")->[0];
    ok $textarea;
    my $hidden = $nodes->("div.hero-unit form input[name='csrf_token']")->[0];
    ok $hidden;
    ok $hidden->attr("value");

    my @content = @words[ 0 .. 100 ];
    my $content = join("\r\n", @content);
    $res = request $cb, POST "http://localhost/post",
        [ content => $content, csrf_token => $hidden->attr("value") ];
    is $res->code, 302;
    ok $res->header("Location");
    sleep 1;
    my $path = URI->new($res->header("Location"))->path;

    $res = request $cb, GET $res->header("Location");
    is $res->code, 200;
    $nodes = findnodes($res->content);
    my $pre = $nodes->("div.hero-unit pre")->[0];
    ok $pre;
    is $pre->as_text => $content;
    my $links = $nodes->("div.row-fluid div.span3 li a");
    my (@link) = grep { $_->attr("href") eq $path } @$links;
    is scalar @link, 1;
    like $link[0]->as_trimmed_text => qr{by test$$};

    # post star+1
    my $next;
    for ( 1 .. 10 ) {
        $form = $nodes->("div.hero-unit form")->[0];
        ok $form;
        $hidden = $nodes->("div.hero-unit form input[name='csrf_token']")->[0];
        ok $hidden;
        ok $hidden->attr("value");

        my $url = "http://localhost" . $form->attr("action");
        $res = request $cb, POST $url, [ csrf_token => $hidden->attr("value") ];
        is $res->code, 302;
        ok $next = $res->header("Location");
    }
    sleep 1;

    $res = request $cb, GET $res->header("Location");
    is $res->code, 200;
    $nodes = findnodes($res->content);
    $pre = $nodes->("div.hero-unit pre")->[0];
    ok $pre;
    is $pre->as_text => $content;

    $links = $nodes->("div.row-fluid div.span3 li a");
    note $links->[0]->as_HTML;
    (@link) = grep { $_->attr("href") eq $path } @$links;
    is scalar @link, 1;
    if ( $link[0]->as_HTML =~ qr{<i class="icon-star"></i>(\d+)} ) {
        my $stars = $1;
        ok $stars >= 10;
    }
};

done_testing;
