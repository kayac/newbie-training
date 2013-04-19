use strict;
use warnings;
use Plack::Test;
use Test::More;
use HTTP::Request::Common;
use t::Util;

my ($app, $mysqld) = setup;

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

subtest_psgi "top", $app, sub {
    my $cb = shift;

    my $res = request $cb, GET "http://localhost/";

    my $nodes = findnodes($res->content);
    my $form = $nodes->("div.hero-unit form")->[0];
    ok $form;
    is $form->attr("action") => "/post";
    is $form->attr("method") => "post";
    my $textarea = $nodes->("div.hero-unit form textarea[name='content']")->[0];
    ok $textarea;
};

done_testing;
