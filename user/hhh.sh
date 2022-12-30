#!/bin/bash


for i in *.{c,h}; do
    vim -c "normal gg=G" -c "wq" $i
done

