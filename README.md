# UIUC-CS425

## MP4 Maple Juice + SQL

To compile and run the Maple Juice System:
```
cd MP4
make
./mj [machine_index]
```

[machine_index] is the indenx of the current machine. It should be a number between 0-9.

You can enter `LEAVE` in the command line to leave the group.

You can enter `CHANGE` in the command line to change the suspicion mode while the sercive is running.

You can enter ` [maple_exe] [num_maples] [sdfs_intermediate_filename_prefix] [sdfs_src_directory]` to initiate a maple task. [num_maples] should be an integer.

You can enter ` [juice_exe] [num_juices] [sdfs_intermediate_filename_prefix] [sdfs_src_directory][delete_output]` to initiate a maple task. [num_juices] should be an integer. [delete_output] should be an bool value.

You can enter `SELECT ALL FROM [dataset] WHRER [field0] [regex0] AND [field1] [regex1]...` to start a filter on the dataset.

You can enter `SELECT ALL FROM [dataset0] [dataset1] WHRER [dataset0.field0] = [dataset1.field1] ` to start a join request on the dataset.
