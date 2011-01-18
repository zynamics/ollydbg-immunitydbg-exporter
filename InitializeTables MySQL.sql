drop table if exists "ex_?_address_comments";
drop table if exists "ex_?_address_references";
drop table if exists "ex_?_expression_substitutions";
drop table if exists "ex_?_operands";
drop table if exists "ex_?_expression_tree_nodes";
drop table if exists "ex_?_expression_trees";
drop table if exists "ex_?_expression_nodes";
drop table if exists "ex_?_control_flow_graphs";
drop table if exists "ex_?_callgraph";
drop table if exists "ex_?_instructions";
drop table if exists "ex_?_basic_blocks";
drop table if exists "ex_?_functions";

create table "ex_?_functions" (
"address" bigint unsigned not null unique,
"name" text not null,
"has_real_name" boolean not null,
"type" int unsigned not null default 0 check( "type" <= 3 ),
"module_name" text null default null,
primary key( "address" )
) ENGINE=InnoDB;

create table "ex_?_basic_blocks" (
"id" int unsigned not null,
"parent_function" bigint unsigned not null,
"address" bigint unsigned not null,
primary key( "id", "parent_function" ),
key( "address" ),
foreign key( "parent_function" ) references "ex_?_functions"( "address" ) on delete cascade on update cascade
) ENGINE=InnoDB;

create table "ex_?_instructions" (
"address" bigint unsigned not null,
"basic_block_id" int unsigned not null,
"mnemonic" varchar( 32 ),
"sequence" int unsigned not null,
"data" blob not null,
primary key( "address", "basic_block_id" ),
foreign key( "basic_block_id" ) references "ex_?_basic_blocks"( "id" ) on delete cascade on update cascade
) ENGINE=InnoDB;

create table "ex_?_callgraph" (
"id" int unsigned not null unique auto_increment,
"source" bigint unsigned not null,
"source_basic_block_id" int unsigned not null,
"source_address" bigint unsigned not null,
"destination" bigint unsigned not null,
primary key( "id" ),
foreign key( "source" ) references "ex_?_functions"( "address" ) on delete cascade on update cascade,
foreign key( "destination" ) references "ex_?_functions"( "address" ) on delete cascade on update cascade,
foreign key( "source_basic_block_id" ) references "ex_?_basic_blocks"( "id" ) on delete cascade on update cascade,
foreign key( "source_address" ) references "ex_?_instructions"( "address" ) on delete cascade on update cascade
) ENGINE=InnoDB;

create table "ex_?_control_flow_graphs" (
"id" int unsigned not null unique auto_increment,
"parent_function" bigint unsigned not null,
"source" int unsigned not null,
"destination" int unsigned not null,
"type" int unsigned not null default 0 check( "type" <= 3 ),
primary key( "id" ),
foreign key( "parent_function" ) references "ex_?_functions"( "address" ) on delete cascade on update cascade,
foreign key( "source" ) references "ex_?_basic_blocks"( "id" ) on delete cascade on update cascade,
foreign key( "destination" ) references "ex_?_basic_blocks"( "id" ) on delete cascade on update cascade
) ENGINE=InnoDB;

create table "ex_?_expression_trees" (
"id" int unsigned not null unique auto_increment,
primary key ( "id" )
) ENGINE=InnoDB;

create table "ex_?_expression_nodes" (
"id" int unsigned not null unique auto_increment,
"type" int unsigned not null default 0 check( "type" <= 7 ),
"symbol" varchar( 256 ),
"immediate" bigint signed,
"position" int,
"parent_id" int unsigned check( "id" > "parent_id" ),
primary key ( "id" ),
foreign key ( "parent_id" ) references "ex_?_expression_nodes"( "id" ) on delete cascade on update cascade
) ENGINE=InnoDB;

create table "ex_?_expression_tree_nodes" (
"expression_tree_id" int unsigned not null,
"expression_node_id" int unsigned not null,
foreign key ( "expression_tree_id" ) references "ex_?_expression_trees"( "id" ) on delete cascade on update cascade,
foreign key ( "expression_node_id" ) references "ex_?_expression_nodes"( "id" ) on delete cascade on update cascade
) ENGINE=InnoDB;

create table "ex_?_operands" (
"address" bigint unsigned not null,
"expression_tree_id" int unsigned not null,
"position" int unsigned not null,
foreign key ( "expression_tree_id" ) references "ex_?_expression_trees"( "id" ) on delete cascade on update cascade,
foreign key ( "address" ) references "ex_?_instructions"( "address" ) on delete cascade on update cascade,
primary key ( "address", "position" )
) ENGINE=InnoDB;

create table "ex_?_expression_substitutions" (
"id" int unsigned not null unique auto_increment,
"address" bigint unsigned not null,
"position" int unsigned not null,
"expression_node_id" int unsigned not null,
"replacement" text not null,
foreign key ( "address", "position" ) references "ex_?_operands"( "address", "position" ) on delete cascade on update cascade,
foreign key ( "expression_node_id" ) references "ex_?_expression_nodes"( "id" ) on delete cascade on update cascade
) ENGINE=InnoDB;

create table "ex_?_address_references" (
"address" bigint unsigned not null,
"position" int unsigned null,
"expression_node_id" int unsigned null,
"destination" bigint unsigned not null,
"type" int unsigned not null default 0 check( "type" <= 8 ),
foreign key ( "address", "position" ) references "ex_?_operands"( "address", "position" ) on delete cascade on update cascade,
foreign key ( "expression_node_id" ) references "ex_?_expression_nodes"( "id" ) on delete cascade on update cascade,
key ( "destination" ),
key ( "type" )
) ENGINE=InnoDB;

create table "ex_?_address_comments" (
"address" bigint unsigned not null unique,
"comment" text not null,
primary key( "address" )
) ENGINE=InnoDB;
