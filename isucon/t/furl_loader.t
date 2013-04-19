use 5.12.0;
use strict;
use warnings;
use Plack::Test;
use Test::More;
use HTTP::Request::Common;
use List::Util qw/ shuffle /;
use Path::Class qw/ file /;
use Time::HiRes qw/ sleep /;
use String::Random;
use t::Util;

my ($app, $mysqld) = setup;
my $running = 1;
local $SIG{ALRM} = sub {
    alarm 0;
    $running = 0;
};
alarm($ENV{TIMEOUT} || 5);

while ($running) {
    subtest_psgi "top", $app, sub {
        my $cb = shift;
        my $res = request $cb, GET "http://localhost/", Connection => "close";
        is $res->code, "200", "get / ok";
        my $nodes = findnodes($res->content);
        my $links = $nodes->("div.row-fluid div.span3 li a");
        for my $link ( (shuffle @$links)[ 1 .. 10 ] ) {
            return unless $running;
            my $path = $link->attr("href");
            my $res = request $cb, GET "http://localhost$path";
            is $res->code, 200, "get $path ok";
        }
        for my $id ( map { int rand(4000) + 1 } ( 1 .. 10 ) ) {
            return unless $running;
            my $res = request $cb, GET "http://localhost/post/$id";
            is $res->code, 200, "get /post/$id ok";
        }
        my $res = request $cb, GET "http://localhost/post/0";
        is $res->code, 404, "get 404";
    }
}
diag "TotalRequests: " . total_requests;
done_testing;

