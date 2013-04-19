use strict;
use warnings;
use Plack::Test;
use Test::More;
use HTTP::Request;

my $app = require "app.psgi";
isa_ok $app, "CODE";

test_psgi
    app    => $app,
    client => sub {
        my $cb  = shift;
        my $req = HTTP::Request->new(
            GET => "http://localhost/",
        );
        my $res = $cb->($req);
        is $res->code, 200, "code";
        is $res->header("Content-Type") => "text/html; charset=UTF-8", "header";
        like $res->content, qr{<title>form</title>}, "title";
        like $res->content, qr{Hello World}, "text";
    };

test_psgi
    app    => $app,
    client => sub {
        my $cb  = shift;
        my $req = HTTP::Request->new(
            GET => "http://localhost/result?name=Mr%2E%20FooBar",
        );
        my $res = $cb->($req);
        is $res->code, 200, "code";
        is $res->header("Content-Type") => "text/html; charset=UTF-8", "header";
        like $res->content, qr{<title>result</title>}, "title";
        like $res->content, qr{Hello Mr\. FooBar}, "text";
    };

test_psgi
    app    => $app,
    client => sub {
        my $cb  = shift;
        my $req = HTTP::Request->new(
            GET => "http://localhost/xxx",
        );
        my $res = $cb->($req);
        is $res->code, 404, "code";
    };

done_testing;
