## Simple scripts to manipulate rt-app outputs
---
#### Create rt-app gnuplot graphs:
```bash
ls *.plot | while read i; do sed '2a set xtics rotate by 45 right' -i $i; gnuplot $i; done
```
#### Add ops in rt-app from all log:
```bash
ls *.log | while read i; do cat $i | tail -1 | cut -d"=" -f2; done | paste -sd+ - | bc
```
