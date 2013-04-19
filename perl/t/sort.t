use strict;
use warnings;
use Test::More;
use Sorter;
use List;

subtest "init" => sub {
    my $sorter = Sorter->new;
    isa_ok $sorter => 'Sorter';
};

subtest values => sub {
    my $sorter = Sorter->new;
    is_deeply [$sorter->get_values], [];

    $sorter->set_values;
    is_deeply [$sorter->get_values], [];

    $sorter->set_values(1);
    is_deeply [$sorter->get_values], [1];

    $sorter->set_values(1,2,3,4,5);
    is_deeply [$sorter->get_values], [1,2,3,4,5];
};

subtest sort => sub {
    my $sorter = Sorter->new;
    $sorter->sort;
    is_deeply [$sorter->get_values], [];

    $sorter->set_values(1);
    $sorter->sort;
    is_deeply [$sorter->get_values], [1];

    $sorter->set_values(5,4,3,2,1);
    $sorter->sort;
    is_deeply [$sorter->get_values], [1,2,3,4,5];

    $sorter->set_values(-1,-2,-3,-4,-5);
    $sorter->sort;
    is_deeply [$sorter->get_values], [-5,-4,-3,-2,-1];

    $sorter->set_values(1,2,3,4,5);
    $sorter->sort;
    is_deeply [$sorter->get_values], [1,2,3,4,5];

    $sorter->set_values(5,5,4,4,4,3,2,1);
    $sorter->sort;
    is_deeply [$sorter->get_values], [1,2,3,4,4,4,5,5];

    for (0..4) {
        my @random_values = ();
        push(@random_values, int(rand() * 100) - 50)  for 0..99;
        $sorter->set_values(@random_values);
        $sorter->sort;
        is_deeply [$sorter->get_values], [sort { $a <=> $b } @random_values];
    }
};


subtest "pass_test" => sub {
    my $list = List->new;
    isa_ok $list => 'List';
};

done_testing;
