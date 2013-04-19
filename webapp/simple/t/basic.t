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
        like $res->content, qr{Hello World}, "text";
    };

done_testing;
