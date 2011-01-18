drop table if exists ex_?_expression_substitutions;
drop table if exists ex_?_address_references;
drop table if exists ex_?_operand_expressions;
drop table if exists ex_?_expression_tree;
drop table if exists ex_?_operand_tuples;
drop table if exists ex_?_operand_strings;
drop table if exists ex_?_address_comments;
drop table if exists ex_?_control_flow_graph;
drop table if exists ex_?_callgraph;
drop table if exists ex_?_instructions;
drop table if exists ex_?_basic_blocks;
drop table if exists ex_?_functions;

create table ex_?_functions (
address bigint unsigned not null unique,
name text not null,
real_name boolean not null,
function_type int unsigned not null default 0 check( function_type <= 3 ),
module_name text null default null,
primary key( address )
) ENGINE=InnoDB;

create table ex_?_basic_blocks (
id int unsigned not null,
parent_function bigint unsigned not null,
address bigint unsigned not null,
primary key( id, parent_function ),
key( address ),
foreign key( parent_function ) references ex_?_functions( address ) on delete cascade
) ENGINE=InnoDB;

create table ex_?_instructions (
address bigint unsigned not null,
basic_block_id int unsigned not null,
mnemonic varchar( 32 ),
sequence int unsigned not null,
data blob not null,
primary key( address, basic_block_id ),
foreign key( basic_block_id ) references ex_?_basic_blocks( id ) on delete cascade
) ENGINE=InnoDB;

create table ex_?_callgraph (
id int unsigned not null unique auto_increment,
src bigint unsigned not null,
src_basic_block_id int unsigned not null,
src_address bigint unsigned not null,
dst bigint unsigned not null,
primary key( id ),
foreign key( src ) references ex_?_functions( address ) on delete cascade,
foreign key( dst ) references ex_?_functions( address ) on delete cascade,
foreign key( src_basic_block_id ) references ex_?_basic_blocks( id ) on delete cascade,
foreign key( src_address ) references ex_?_instructions( address ) on delete cascade
) ENGINE=InnoDB;

create table ex_?_control_flow_graph (
id int unsigned not null unique auto_increment,
parent_function bigint unsigned not null,
src int unsigned not null,
dst int unsigned not null,
kind int unsigned not null default 0 check( kind <= 3 ),
primary key( id ),
foreign key( parent_function ) references ex_?_functions( address ) on delete cascade,
foreign key( src ) references ex_?_basic_blocks( id ) on delete cascade,
foreign key( dst ) references ex_?_basic_blocks( id ) on delete cascade
) ENGINE=InnoDB;

create index ex_?_control_flow_graph_parent_function_src on ex_?_control_flow_graph( parent_function, src );

create table ex_?_operand_strings (
id int unsigned not null unique auto_increment,
str text not null,
primary key (id)
) ENGINE=InnoDB;

create table ex_?_operand_tuples (
address bigint unsigned not null,
operand_id int unsigned not null,
position int unsigned not null,
foreign key(operand_id) references ex_?_operand_strings( id ) on delete cascade,
foreign key(address) references ex_?_instructions( address ) on delete cascade
) ENGINE=InnoDB;

create table ex_?_expression_tree (
id int unsigned not null unique auto_increment,
expr_type int unsigned not null default 0 check( expr_type <= 7 ),
symbol varchar(256),
immediate bigint signed,
position int,
parent_id int unsigned check( id > parent_id ),
primary key ( id ),
foreign key ( parent_id ) references ex_?_expression_tree( id ) on delete cascade
) ENGINE=InnoDB;

create table ex_?_address_references (
address bigint unsigned not null,
operand_id int unsigned null,
expression_id int unsigned null,
target bigint unsigned not null,
kind int unsigned not null default 0 check( kind <= 8 ),
foreign key (address) references ex_?_instructions(address) on delete cascade,
foreign key (operand_id) references ex_?_operand_strings( id ) on delete cascade,
foreign key (expression_id) references ex_?_expression_tree( id ) on delete cascade,
key ( target ),
key ( kind )
) ENGINE=InnoDB;

create table ex_?_operand_expressions (
operand_id int unsigned not null,
expr_id int unsigned not null,
foreign key (operand_id) references ex_?_operand_strings( id ) on delete cascade,
foreign key (expr_id) references ex_?_expression_tree( id ) on delete cascade
) ENGINE=InnoDB;

create table ex_?_expression_substitutions (
id int unsigned not null unique auto_increment,
address bigint unsigned not null,
operand_id int unsigned not null,
expr_id int unsigned not null,
replacement text not null,
foreign key (address) references ex_?_instructions(address) on delete cascade,
foreign key (operand_id) references ex_?_operand_strings(id) on delete cascade,
foreign key (expr_id) references ex_?_expression_tree(id) on delete cascade
) ENGINE=InnoDB;

create table ex_?_address_comments (
address bigint unsigned not null unique,
comment text not null,
primary key( address )
) ENGINE=InnoDB;
