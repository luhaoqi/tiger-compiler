## lab2 
1. how you handle comments
    注释通过增加一个COMMENT模式，读取到/\*之后进入该模式，同时把comment_level_ +1，用来处理嵌套注释，用一个类似栈的方式，每遇到/\*之后层数+1，遇到\*/层数-1，直到为0退出该模式
2. how you handle strings
   读取到"进入STR模式，在其中遇到\n,\t,\\\\,\\*等都是直接加入临时字符串变量string_buf，退出时setMatched(string_buf_)即可，需要注意使用adjustStr而非adjust，否则报错信息将不是在第一个出错的位置；控制字符因为ASCII从1开始连续分布，只需要加上一个常数就能进行换算；/f___f/只需要用一个正则表达式匹配即可。
3. error handling & end-of-file handling
    在STR与COMMENT模式中adjustStr，从而使得报错位置在开头；另外EOF的判定在INITIAL模式下正常结束，否则需要报错unclosed string/comment.

finished by LuHaoqi