#!/usr/bin/env perl
use 5.12.0;
use strict;
use warnings;
use Amon2::DBI;
use String::Random;
use Log::Minimal;
use List::Util qw/ shuffle /;
use Path::Class qw/ file /;

my $load   = shift || 1;
our @words = shuffle ( map { chomp; $_ } file("/usr/share/dict/words")->slurp );

my $dbh = Amon2::DBI->connect(
    'dbi:mysql:database=test',
    'root',
    '',
);

infof "starting load %d", $load;

for my $n ( 1 .. $load ) {
    $dbh->begin_work;

    my $username = gen_username();
    my $password = gen_password();
    $dbh->do(
        "INSERT INTO users (username, password) VALUES (?, ?)",
        {},
        $username, $password,
    );
    my $user_id = $dbh->{'mysql_insertid'};
    my $posts = int rand(10);
    for ( 1 .. $posts ) {
        my $content = gen_content();
        die "no content" unless $content;
        $dbh->do(
            "INSERT INTO posts (user_id, content) VALUES (?, ?)",
            {},
            $user_id, $content,
        );
        my $post_id = $dbh->{'mysql_insertid'};
        my $stars = int rand(20);
        for ( 1 .. $stars ) {
            $dbh->do(
                "INSERT INTO stars (post_id, user_id) VALUES (?, ?)",
                {},
                $post_id, $user_id,
            );
        }
        infof "%d stars created", $stars;
    }
    infof "%d posts created", $posts;
    infof "%d users loaded", $n;
    $dbh->commit;
}

$dbh->disconnect;

sub gen_username {
    my $time = time;
    state $serial = 0;
    $serial++;
    return "i${time}x$serial";
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

