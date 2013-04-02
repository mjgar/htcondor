##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************

package SimpleJob;

use CondorTest;

$submitted = sub
{
	CondorTest::debug("Job submitted\n",1);
};

$aborted = sub 
{
	die "Abort event NOT expected\n";
};

$dummy = sub
{
};

$ExitSuccess = sub {
	CondorTest::debug("Job completed\n",1);
};

sub RunCheck
{
    my %args = @_;
    my $testname = $args{test_name} || CondorTest::GetDefaultTestName();
    my $universe = $args{universe} || "vanilla";
    my $user_log = $args{user_log} || CondorTest::TempFileName("$testname.user_log");
    my $append_submit_commands = $args{append_submit_commands} || "";
    my $grid_resource = $args{grid_resource} || "";
    my $should_transfer_files = $args{should_transfer_files} || "";
    my $when_to_transfer_output = $args{when_to_transfer_output} || "";
    my $duration = $args{duration} || "1";
    my $execute_fn = $args{on_execute} || $dummy;
    my $ulog_fn = $args{on_ulog} || $dummy;

    CondorTest::RegisterAbort( $testname, $aborted );
    CondorTest::RegisterExitedSuccess( $testname, $ExitSuccess );
    CondorTest::RegisterExecute($testname, $execute_fn);
    CondorTest::RegisterULog($testname, $ulog_fn);
    CondorTest::RegisterSubmit( $testname, $submitted );

    my $submit_fname = CondorTest::TempFileName("$testname.submit");
    open( SUBMIT, ">$submit_fname" ) || die "error writing to $submit_fname: $!\n";
    print SUBMIT "universe = $universe\n";
    print SUBMIT "executable = x_sleep.pl\n";
    print SUBMIT "log = $user_log\n";
    print SUBMIT "arguments = $duration\n";
    print SUBMIT "notification = never\n";
    if( $grid_resource ne "" ) {
	print SUBMIT "GridResource = $grid_resource\n"
    }
    if( $should_transfer_files ne "" ) {
	print SUBMIT "ShouldTransferFiles = $should_transfer_files\n";
    }
    if( $when_to_transfer_output ne "" ) {
	print SUBMIT "WhenToTransferOutput = $when_to_transfer_output\n";
    }
    if( $append_submit_commands ne "" ) {
        print SUBMIT "\n" . $append_submit_commands . "\n";
    }
    print SUBMIT "queue\n";
    close( SUBMIT );

    my $result = CondorTest::RunTest($testname, $submit_fname, 0);
    CondorTest::RegisterResult( $result, %args );
    return $result;
}

1;
