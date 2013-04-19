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
my @words = shuffle ( map { chomp; $_ } file("/usr/share/dict/words")->slurp );

sub gen_username {
    my $time = time;
    return "b${time}x$$";
}

sub gen_password {
    String::Random::random_regex("........");
}

sub gen_content {
    my @content;
    my $max_lines = 100;
    my $i = 0;
    my $w = @words;
    for my $w ( map { $words[int rand $w] } ( 1 .. $max_lines ) ) {
        push @content, $w;
        push @content, "\r\n" if rand(5) < 1;
    }
    join " ", @content;
}

sub idle {
    Time::HiRes::sleep 1;
}

my $time  = time;
my $running = 1;
local $SIG{ALRM} = sub {
    alarm 0;
    $running = 0;
};
alarm($ENV{TIMEOUT} || 5);

while ($running) {
    subtest_psgi "top", $app, sub {
        my $cb = shift;
        my $res = request $cb, GET "http://localhost/";
        is $res->code, 200, "get / ok";
        my $nodes = findnodes($res->content);
        my $link = $nodes->("div.hero-unit p a")->[0];
        is $link->attr("href") => "/signin", "top header link to /signin";
        is $link->as_trimmed_text, "Please sign in", "top header link text 'Please sign in'";
    };
    last unless $running;

    subtest_psgi "signup ok", $app, sub {
        my $cb  = shift;
        my $res = request $cb, GET "http://localhost/signup";
        is $res->code, 200, "get /signup ok";
        my $nodes = findnodes($res->content);
        my $form = $nodes->("form.form-signin")->[0];
        my $hidden = $nodes->("form.form-signin input[name='csrf_token']")->[0];
        ok my $token = $hidden->attr("value"), "signup csrf_token";

        my $username = gen_username();
        my $password = gen_password();
        $res = request(
            $cb,
            POST "http://localhost/signup",
            [
                username         => $username,
                password         => $password,
                password_confirm => $password,
                csrf_token       => $token,
            ],
        );
        is $res->code => 302, "post /signup success";
        my $next = URI->new( $res->header("Location") );

        $res = request $cb, GET $next;
        $nodes = findnodes($res->content);
        my $p = $nodes->("p.navbar-text")->[0];
        like $p->as_trimmed_text, qr/Logged in as \Q$username\E/, "header logged in";
    };
    last unless $running;

    subtest_psgi "top_post_star", $app, sub {
        my $cb = shift;

        my $res = request $cb, GET "http://localhost/";
        my $nodes = findnodes($res->content);
        my $hidden = $nodes->("div.hero-unit form input[name='csrf_token']")->[0];
        ok my $token = $hidden->attr("value"), "get /post csrf_token";

        my $content = gen_content();
        $res = request $cb, POST "http://localhost/post",
            [ content => $content, csrf_token => $token ];
        is $res->code, 302, "post /post success";
        my $next = URI->new( $res->header("Location") );
        my $post_path = $next->path;
        idle();

        $res = request $cb, GET $next;
        is $res->code, 200;
        $nodes = findnodes($res->content);
        my $pre = $nodes->("div.hero-unit pre")->[0];
        is $pre->as_text => $content, "get $post_path includes content";

        # sidebar
        my $links = $nodes->("div.row-fluid div.span3 li a");
        is scalar @$links => 100, "sidebar links num";
        my (@link) = grep { $_->attr("href") eq $post_path } @$links;
        is scalar @link, 1, "get $post_path includes path in sidebar";
        my $headline = substr($content, 0, 10);
        $headline =~ s/[\r\n ]+/ /mg;
        like $link[0]->as_trimmed_text => qr{^\Q$headline\E.*by}, "get $post_path includes headline of content";

        {
            # サイドバーの末尾
            my $bottom = $link[-1];
            $res = request $cb, GET "http://localhost" . $bottom->attr("href");
            is $res->code, 200;
            my $s_nodes = findnodes($res->content);
            # sidebar
            my $links = $s_nodes->("div.row-fluid div.span3 li a");
            my (@link) = grep { $_->attr("href") eq $post_path } @$links;
            is scalar @link, 1, "get $post_path includes path in sidebar";
            my $headline = substr($content, 0, 10);
            $headline =~ s/[\r\n ]+/ /mg;
            like $link[0]->as_trimmed_text => qr{^\Q$headline\E.*by}, "get $post_path includes headline of content";
        }

        my $form = $nodes->("div.hero-unit form")->[0];
        $hidden = $nodes->("div.hero-unit form input[name='csrf_token']")->[0];
        ok $token = $hidden->attr("value"), "get $post_path csrf_token";

        # post star+1
        my $add_stars = int rand(20);
        for ( 1 .. $add_stars ) {
            return unless $running;
            my $star_path = $form->attr("action");
            my $url = "http://localhost$star_path";
            $res = request $cb, POST $url, [ csrf_token => $token ];
            is $res->code, 302, "post $star_path success";
        }
        idle();

        $res = request $cb, GET "http://localhost/";
        is $res->code, 200, "get / ok";
        $nodes = findnodes($res->content);
        $links = $nodes->("div.row-fluid div.span3 li a");

        (@link) = grep { $_->attr("href") eq $post_path } @$links;
        is scalar @link, 1, "get / sidebar includes $post_path";
        if ( $link[0]->as_HTML =~ qr{<i class="icon-star"></i>(\d+)} ) {
            my $stars = $1;
            ok $stars >= $add_stars, "get / sidebar stars ok";
        }
    };
    last unless $running;

    subtest_psgi "signout", $app, sub {
        my $cb  = shift;
        my $res = request $cb, GET "http://localhost/signout";
        is $res->code, 302, "get /signout ok";
        my $next = URI->new( $res->header("Location") );

        $res = request $cb, GET $next, Connection => "close";
        is $res->code => 200, "get / ok";
        my $nodes = findnodes($res->content);
        my $p = $nodes->("p.navbar-text")->[0];
        like $p->as_trimmed_text, qr/Sign in/, "get / logged out";
    };
}

diag "TotalRequests: " . total_requests;
done_testing;

