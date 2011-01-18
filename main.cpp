#include <zylibcpp/utility/XmlConfig.hpp>
#include <zylibcpp/utility/Utility.hpp>

#pragma warning ( push, 0 )
#include <map>
#include <cmath>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <boost/timer.hpp>
#include <boost/algorithm/string.hpp>
#include <Windows.h>
#include <soci.h>
#include <soci-mysql.h>
#ifdef IMMUNITY
#define FunctionPlugindata 			IMMDBG_Plugindata 		
#define FunctionPlugininit			IMMDBG_Plugininit		
#define FunctionPluginmainloop		IMMDBG_Pluginmainloop	
#define FunctionPluginsaveudd		IMMDBG_Pluginsaveudd	
#define FunctionPluginuddrecord		IMMDBG_Pluginuddrecord	
#define FunctionPluginmenu			IMMDBG_Pluginmenu		
#define FunctionPluginaction		IMMDBG_Pluginaction	
#define FunctionPluginshortcut		IMMDBG_Pluginshortcut	
#define FunctionPluginreset			IMMDBG_Pluginreset		
#define FunctionPluginclose			IMMDBG_Pluginclose		
#define FunctionPlugindestroy		IMMDBG_Plugindestroy	
#define FunctionPlugincmd			IMMDBG_Plugincmd		
#include "ImmunityDbg/plugin.h"
#pragma comment ( lib, "ImmunityDebugger.lib" )
#else
#define FunctionPlugindata 			ODBG_Plugindata 		
#define FunctionPlugininit			ODBG_Plugininit		
#define FunctionPluginmainloop		ODBG_Pluginmainloop	
#define FunctionPluginsaveudd		ODBG_Pluginsaveudd	
#define FunctionPluginuddrecord		ODBG_Pluginuddrecord	
#define FunctionPluginmenu			ODBG_Pluginmenu		
#define FunctionPluginaction		ODBG_Pluginaction	
#define FunctionPluginshortcut		ODBG_Pluginshortcut	
#define FunctionPluginreset			ODBG_Pluginreset		
#define FunctionPluginclose			ODBG_Pluginclose		
#define FunctionPlugindestroy		ODBG_Plugindestroy	
#define FunctionPlugincmd			ODBG_Plugincmd		
#include "OllyDbg/plugin.h"	// OllyDbg
#pragma comment ( lib, "OLLYDBG.LIB" )
#endif
#pragma warning ( pop )

// paths are relative to project dir
// must be in source file since project linker settings don't support conditionals (USE_DATABASE)
#if _MSC_VER >= 1600
#	ifdef _DEBUG
#		pragma comment( lib, "..\\external\\soci\\lib\\libsoci_core-vc100-d-3_0.lib" )
#		pragma comment( lib, "..\\external\\soci\\lib\\libsoci_mysql-vc100-d-3_0.lib" )
#	else
#		pragma comment( lib, "..\\external\\soci\\lib\\libsoci_core-vc100-3_0.lib" )
#		pragma comment( lib, "..\\external\\soci\\lib\\libsoci_mysql-vc100-3_0.lib" )
#	endif
#else
#	ifdef _DEBUG
#		pragma comment( lib, "..\\external\\soci\\lib\\libsoci_core-vc80-d-3_0.lib" )
#		pragma comment( lib, "..\\external\\soci\\lib\\libsoci_mysql-vc80-d-3_0.lib" )
#	else
#		pragma comment( lib, "..\\external\\soci\\lib\\libsoci_core-vc80-3_0.lib" )
#		pragma comment( lib, "..\\external\\soci\\lib\\libsoci_mysql-vc80-3_0.lib" )
#	endif
#endif 

#include "main.hpp"
#undef max

soci::session g_Database;


// -----------------------------------------------------------------------------


static const std::string g_VersionString(
	std::string( "Zynamics GmbH ")
#ifdef IMMUNITY
	+ std::string( "ImmunityDebugger" )
#else
	+ std::string( "OllyDbg" )
#endif
	+ std::string(" Exporter (" ) + __DATE__ + std::string( ")" ));
CXmlConfig g_XmlConfig( "ZynamicsExporter.xml", "ZynamicsExporter" );
std::ofstream g_LogFile( "ZynamicsExporter.log" );
static const bool g_Logging = g_XmlConfig.readBool( "//LoggingActive/@value", true );
CTerminator flushQuery;


// -----------------------------------------------------------------------------


boost::int64_t
readNumber( const std::string & text )
{
	const bool isNegative = text.find( '-' ) != std::string::npos;
	std::stringstream stream;
	if ( isNegative )
		stream << text.substr( 1 );
	else
		stream << text;
	boost::int64_t number;
	stream >> std::hex >> number;
	return isNegative ? -number : number;
}


void
executeSql( const std::string & query )
{
	if ( g_Logging )
		g_LogFile << query << ";" << std::endl;
	g_Database << query;
}


std::string
getFunctionName( const ulong address )
{
	std::stringstream name;
	char nameBuffer[TEXTLEN];
	memset( nameBuffer, 0, TEXTLEN );
	if ( Findname( address, NM_ANYNAME, nameBuffer ) == NM_NONAME )
		name << "sub_" << std::setfill( '0' ) << std::setw( 8 ) << std::setbase( 16 ) << address;
	else
		name << nameBuffer;
	return name.str();
}


std::ostream &
operator << ( std::ostream & stream, const CExpressionTreeNode & node )
{
	stream // note: no id is streamed, because we use this string to determine uniqueness
		<< node.m_Type << " "
		<< node.m_Symbol << " "
		<< node.m_Immediate << " "
		<< node.m_Position << " "
		<< node.m_Parent;
	return stream;
}


// -----------------------------------------------------------------------------


CQueryBuilder::CQueryBuilder( std::stringstream & basequery, size_t querySize )
	: m_BaseQuery( basequery.str())
	, m_QuerySize( querySize != 0 ? querySize : g_XmlConfig.readInt( "//QuerySize/@value", 1024 * 1024 ))
{
	m_CurrentQuery << m_BaseQuery;
}


void
CQueryBuilder::execute()
{
	for ( std::vector< std::string >::const_iterator i = m_Queries.begin(); i != m_Queries.end(); ++i )
		executeSql( *i );
	if ( m_CurrentQuery.str() != m_BaseQuery )
		executeSql( m_CurrentQuery.str().substr( 0, m_CurrentQuery.str().size() - 1 ));
}


CQueryBuilder &
operator << ( CQueryBuilder & builder, const CTerminator & )
{
	if ((size_t)builder.m_CurrentQuery.tellp() >= builder.m_QuerySize - 1024 * 64 )	// 4MB
	{	// buffer overrun, create new query
		builder.m_Queries.push_back( builder.m_CurrentQuery.str().substr( 0, builder.m_CurrentQuery.str().size() - 1 ));
		builder.m_CurrentQuery.str( "" );
		builder.m_CurrentQuery << builder.m_BaseQuery;
	}
	return builder;
}


CQueryBuilder &
operator << ( CQueryBuilder & builder, const std::string & query )
{	
	builder.m_CurrentQuery << query;
	return builder;
}


CQueryBuilder &
operator << ( CQueryBuilder & builder, ulong value )
{
	builder.m_CurrentQuery << value;
	return builder;
}


// -----------------------------------------------------------------------------


ulong CBasicBlock::ms_NextId = 0;


CBasicBlock::CBasicBlock()
	: m_Address( 0 )
	, m_Id( 0 )
{
}


CBasicBlock::CBasicBlock( ulong startAddress )
	: m_Address( startAddress )
	, m_Id( 0 )
{
}


void
CBasicBlock::nextId()
{
	m_Id = ++ms_NextId;
}


// -----------------------------------------------------------------------------


CExpressionTreeNode::TTypeTable
CExpressionTreeNode::initTypeTable()
{
	TTypeTable table;
	table["EAX"] = TYPE_REGISTER;
	table["EBX"] = TYPE_REGISTER;
	table["ECX"] = TYPE_REGISTER;
	table["EDX"] = TYPE_REGISTER;
	table["ESP"] = TYPE_REGISTER;
	table["EBP"] = TYPE_REGISTER;
	table["ESI"] = TYPE_REGISTER;
	table["EDI"] = TYPE_REGISTER;
	table["EIP"] = TYPE_REGISTER;

	table["AX"] = TYPE_REGISTER;
	table["BX"] = TYPE_REGISTER;
	table["CX"] = TYPE_REGISTER;
	table["DX"] = TYPE_REGISTER;

	table["AL"] = TYPE_REGISTER;
	table["BL"] = TYPE_REGISTER;
	table["CL"] = TYPE_REGISTER;
	table["DL"] = TYPE_REGISTER;

	table["AH"] = TYPE_REGISTER;
	table["BH"] = TYPE_REGISTER;
	table["CH"] = TYPE_REGISTER;
	table["DH"] = TYPE_REGISTER;

	table["CS"] = TYPE_REGISTER;
	table["DS"] = TYPE_REGISTER;
	table["ES"] = TYPE_REGISTER;
	table["FS"] = TYPE_REGISTER;
	table["GS"] = TYPE_REGISTER;
	table["SS"] = TYPE_REGISTER;

	table["SI"] = TYPE_REGISTER;
	table["DI"] = TYPE_REGISTER;
	table["BP"] = TYPE_REGISTER;
	table["SP"] = TYPE_REGISTER;

	table["ST"]	   = TYPE_REGISTER;
	table["ST(0)"] = TYPE_REGISTER;
	table["ST(1)"] = TYPE_REGISTER;
	table["ST(2)"] = TYPE_REGISTER;
	table["ST(3)"] = TYPE_REGISTER;
	table["ST(4)"] = TYPE_REGISTER;
	table["ST(5)"] = TYPE_REGISTER;
	table["ST(6)"] = TYPE_REGISTER;
	table["ST(7)"] = TYPE_REGISTER;

	table["MM0"] = TYPE_REGISTER;
	table["MM1"] = TYPE_REGISTER;
	table["MM2"] = TYPE_REGISTER;
	table["MM3"] = TYPE_REGISTER;
	table["MM4"] = TYPE_REGISTER;
	table["MM5"] = TYPE_REGISTER;
	table["MM6"] = TYPE_REGISTER;
	table["MM7"] = TYPE_REGISTER;

	// SSE registers?!
	return table;
}


CExpressionTreeNode::TTypeTranslation
CExpressionTreeNode::initTypeTranslation()
{
	TTypeTranslation table;
	table["QWORD"] = "b8";
	table["DWORD"] = "b4";
	table["WORD"]  = "b2";
	table["BYTE"]  = "b1";
	return table;
}


CExpressionTreeNode::TTypeTable CExpressionTreeNode::ms_TypeTable = CExpressionTreeNode::initTypeTable();
CExpressionTreeNode::TTypeTranslation CExpressionTreeNode::ms_TypeTranslation = CExpressionTreeNode::initTypeTranslation();


CExpressionTreeNode::CExpressionTreeNode()
	: m_Id( 0 )
	, m_Type( 0 )
	, m_Symbol( "" )
	, m_Immediate( 0 )
	, m_Position( 0 )
	, m_Parent( 0 )
{
}


CExpressionTreeNode::CExpressionTreeNode( unsigned id, int type, const std::string & symbol, boost::int64_t immediate, int position, unsigned parent	)
	: m_Id( id )
	, m_Type( type )
	, m_Symbol( symbol )
	, m_Immediate( immediate )
	, m_Position( position )
	, m_Parent( parent )
{
}


// -----------------------------------------------------------------------------


CFunction::CFunction(
	ulong startAddress, ulong endAddress, const std::string & name, const std::string & module,
	int moduleId, CExporter * exporter, CQueryBuilder & instructionQuery, CQueryBuilder & basicBlockQuery,
	CQueryBuilder & flowgraphQuery, unsigned type )
	: m_Name( name )
	, m_Module( module )
	, m_StartAddress( startAddress )
	, m_EndAddress( endAddress )
	, m_Type( type )
	, m_ModuleId( moduleId )
	, m_Exporter( exporter )
	, m_InstructionQuery( instructionQuery )
	, m_BasicBlockQuery( basicBlockQuery )
	, m_FlowGraphQuery( flowgraphQuery )
{
	// endAddress points to the last command, _not_ the start of the next as it should
	// we fix that here:
	ulong size;
	Finddecode( m_EndAddress, &size );
	uchar currentCommand[MAXCMDSIZE];
	Readcommand( m_EndAddress, (char *)currentCommand );
	const ulong blockSize = endAddress - startAddress;
	m_EndAddress = Disassembleforward( NULL, m_EndAddress, blockSize, m_EndAddress, 1, 1 );
}


// returns the expression id referencing a call/jmp address if any
ulong
CFunction::decodeLine(
	const t_disasm & assemblyLine,
	CQueryBuilder & tupleQuery,
	CQueryBuilder & stringQuery,
	ulong & operandId )
{
	std::string line( assemblyLine.result );
	std::size_t mnemonicTerminator = line.find( ' ' );
	if ( line.find( "REP" ) == 0 || line.find( "rep" ) == 0 )	// special case, REP is an operator prefix separated by space
		mnemonicTerminator = line.find( ' ', mnemonicTerminator + 1 );
	std::string mnemonic( line.substr( 0, mnemonicTerminator ));
	std::string dump( assemblyLine.dump );
	boost::replace_all( dump, " ", "" );
	boost::replace_all( dump, ":", "" );
	m_Lines.push_back( CLine( assemblyLine.ip, mnemonic, dump,
		assemblyLine.cmdtype != C_RET &&
		assemblyLine.cmdtype != C_JMP &&
		assemblyLine.cmdtype != C_JMC ));

	line = line.substr( mnemonicTerminator + 1 );
	if ( mnemonicTerminator == std::string::npos )
		return 0;	// just the mnemonic - no operands to parse

	size_t pos = line.find( ',' );
	int lastpos = -1;
	int position = -1;
	ulong expressionId = 0;
	do
	{
		std::string token = line.substr( lastpos + 1, pos - lastpos - 1 );
		expressionId = m_Exporter->decodeOperand( token, 0, ++operandId, assemblyLine );
		tupleQuery << "(" << assemblyLine.ip << "," << operandId << "," << (++position) << ")," << flushQuery;

		stringQuery << "('" << token << "')," << flushQuery;
		lastpos = int( pos );
		pos = line.find( ',', pos + 1 );
	}
	while ( size_t( lastpos ) != std::string::npos );
	return expressionId;
}


void
CFunction::addBasicBlock( const CBasicBlock & basicBlock )
{
	m_BasicBlocks[basicBlock.m_Address] = basicBlock;
}


bool
CFunction::hasRealName() const
{
	return m_Name.find( "sub_" ) != 0;
}


void		
CFunction::addEdge( ulong source, ulong sourceOperand, ulong sourceExpression, ulong dest, CEdge::TEdgeType type )
{
	m_Edges.push_back( CEdge( source, sourceOperand, sourceExpression, dest, type ));
}


unsigned	
CFunction::getNrOfBasicBlocks() const
{
	return unsigned int( m_BasicBlocks.size());
}


void
CFunction::writeStub()
{
	// stub function for dll call. Insert dummy basic block and instruction
	CBasicBlock basicBlock( getAddress());
	basicBlock.nextId();
	m_BasicBlockQuery
		<< "(" << basicBlock.m_Id
		<< "," << getAddress()
		<< ", " << basicBlock.m_Address << ")," << flushQuery;

	m_InstructionQuery
		<< "(" << getAddress()
		<< "," << basicBlock.m_Id
		<< ",null,0,'')," << flushQuery;
}


void
CFunction::write()
{
	if ( !m_BasicBlocks.empty())
	{
		for ( TBasicBlocks::const_iterator i = m_BasicBlocks.begin(); i != m_BasicBlocks.end(); ++i )
		{
			const CBasicBlock & basicBlock = i->second;
			m_BasicBlockQuery
				<< "(" << basicBlock.m_Id
				<< "," << getAddress()
				<< ", " << basicBlock.m_Address << ")," << flushQuery;
		}
	}

	// @bug: the case in which lines is not empty but basicblocks is, is an export error and should never happen
	if ( !m_Lines.empty() && !m_BasicBlocks.empty())
	{
		TBasicBlocks::const_iterator basicBlock = m_BasicBlocks.begin();
		TBasicBlocks::const_iterator nextBasicBlock = basicBlock;
		++nextBasicBlock;
		ulong sequence = 0;
		for ( TLines::const_iterator i = m_Lines.begin(); i != m_Lines.end(); ++i, ++sequence )
		{
			if ( nextBasicBlock != m_BasicBlocks.end()
				&& i->m_Address >= nextBasicBlock->second.m_Address )
			{
				basicBlock = nextBasicBlock;
				++nextBasicBlock;
				sequence = 0;

				// insert flow edges
				TLines::const_iterator previous = i;
				if ( previous != m_Lines.end() && previous != m_Lines.begin())
				{
					--previous;
					if ( previous->m_IsFlow )
						m_Edges.push_back( CEdge( previous->m_Address, 0, 0, i->m_Address, CEdge::TYPE_UNCONDITIONAL ));
				}
			}

			m_InstructionQuery
				<< "(" << i->m_Address
				<< "," << basicBlock->second.m_Id
				<< ",'" << boost::to_lower_copy( i->m_Mnemonic )
				<< "'," << sequence << ",x'" << i->m_BinaryDump
				<< "')," << flushQuery;
		}
	}

	resolveEdges();
	if ( !m_Edges.empty())
	{
		for ( TEdges::const_iterator i = m_Edges.begin(); i != m_Edges.end(); ++i )
			m_FlowGraphQuery << "(" << getAddress() << "," << i->m_Source << "," << i->m_Dest << "," << i->m_Type << ")," << flushQuery;
	}
}


void
CFunction::resolveAddressesToIds()
{
	if ( m_BasicBlocks.empty())
		return;	// can happen if we are in a stub function. They get dummy basic blocks created only later...

	for ( TBasicBlocks::iterator i = m_BasicBlocks.begin(); i != m_BasicBlocks.end(); ++i )
		i->second.nextId();
}


const CBasicBlock *
CFunction::getBasicBlockForAddress( ulong address ) const
{
	if ( m_BasicBlocks.empty())
		return 0;

	TBasicBlocks::const_iterator sourceNode = m_BasicBlocks.upper_bound( address );
	if ( sourceNode == m_BasicBlocks.begin())
	{	// target address lies before our first basicblock, i.e. outside our function
		return 0;
	}
	else if ( sourceNode != m_BasicBlocks.end())
	{	// address is inside our function
		--sourceNode;
		return &sourceNode->second;
	}
	else if ( address <= m_EndAddress && address >= m_StartAddress )
	{	// address is inside our function but no basicblock follows it
		return &m_BasicBlocks.rbegin()->second;
	}
	return 0;
}


void
CFunction::resolveEdges()
{
	for ( TEdges::iterator i = m_Edges.begin(); i != m_Edges.end(); ++i )
	{
		ulong source = i->m_Source;
		ulong dest = i->m_Dest;
		const CBasicBlock * basicBlock = getBasicBlockForAddress( source );
		if ( basicBlock )
		{
			i->m_Source = basicBlock->m_Id;
			TBasicBlocks::const_iterator j = m_BasicBlocks.find( dest );
			if ( j != m_BasicBlocks.end())
				i->m_Dest = j->second.m_Id;
			else	// @bug: this should never happen!
				i->m_Dest = i->m_Source;	
		}
		else
		{	// @bug: this is an error -> should never happen!
			i->m_Source = 1;
			i->m_Dest = 1;
		}
	}
}


bool
CFunction::addressInFunction( ulong address ) const
{
	return address >= m_StartAddress && address <= m_EndAddress;
}


std::string
CFunction::getName() const
{
	return m_Name;
}


std::string
CFunction::getModule() const
{
	return m_Module;
}


ulong		
CFunction::getAddress() const
{
	return m_StartAddress;
}


ulong		
CFunction::getEndAddress() const
{
	return m_EndAddress;
}


unsigned
CFunction::getType() const
{
	return m_Type;
}


// -----------------------------------------------------------------------------


CExporter::CExporter()
	: m_DllInstance( 0 )
	, m_WindowHandle( 0 )
	, m_ModuleId()
	, m_ExpresssionTreeNodeId( 0 )
	, m_OperandTupleQuery( 0 )
	, m_OperandStringQuery( 0 )
	, m_OperandExpressionQuery( 0 )
	, m_InstructionQuery( 0 )
	, m_BasicBlockQuery( 0 )
	, m_FlowGraphQuery( 0 )
	, m_ExpressionSubstitutionQuery( 0 )

{
	reset();
}


CExporter::~CExporter()
{
	reset();
	delete m_OperandTupleQuery;
	delete m_OperandStringQuery;
	delete m_OperandExpressionQuery;
	delete m_InstructionQuery;
	delete m_BasicBlockQuery;
	delete m_FlowGraphQuery;
	delete m_ExpressionSubstitutionQuery;
}


void
CExporter::setDatabaseName( const std::string & name )
{
	m_DatabaseName = name;
}


void
CExporter::reset()
{
	for ( TFunctions::const_iterator i = m_Functions.begin(); i != m_Functions.end(); ++i )
		delete i->second;

	m_AllEdges.clear();
	m_Functions.clear();
	m_CallGraphEdges.clear();
	m_ExpressionTreeNodes.clear();
	m_ExpresssionTreeNodeId = 0;

	{
	std::stringstream query;
	query << "insert into ex_" << m_ModuleId << "_operand_tuples (address, operand_id, position) values ";
	delete m_OperandTupleQuery;
	m_OperandTupleQuery = new CQueryBuilder( query );
	}

	{
	std::stringstream query;
	query << "insert into ex_" << m_ModuleId << "_operand_strings (str) values ";
	delete m_OperandStringQuery;
	m_OperandStringQuery = new CQueryBuilder( query );
	}

	{
	std::stringstream query;
	query << "insert into ex_" << m_ModuleId << "_operand_expressions (operand_id,expr_id) values ";
	delete m_OperandExpressionQuery;
	m_OperandExpressionQuery = new CQueryBuilder( query );
	}

	{
	std::stringstream query;
	query << "insert into ex_" << m_ModuleId << "_basic_blocks values ";
	delete m_BasicBlockQuery;
	m_BasicBlockQuery = new CQueryBuilder( query );
	}

	{
	std::stringstream query;
	query << "insert into ex_" << m_ModuleId << "_instructions values ";
	delete m_InstructionQuery;
	m_InstructionQuery = new CQueryBuilder( query );
	}

	{
	std::stringstream query;
	query << "insert into ex_" << m_ModuleId << "_control_flow_graph (parent_function, src, dst, kind) values ";
	delete m_FlowGraphQuery;
	m_FlowGraphQuery = new CQueryBuilder( query );
	}	

	{
	std::stringstream query;
	query << "insert into ex_" << m_ModuleId << "_expression_substitutions (address,operand_id,expr_id,replacement) values ";
	delete m_ExpressionSubstitutionQuery;
	m_ExpressionSubstitutionQuery = new CQueryBuilder( query );
	}	
	
	m_OperandId = 0;
}


CExpressionTreeNode *
CExporter::getId( CExpressionTreeNode & node )
{
	std::stringstream text;
	text << node;
	if ( m_ExpressionTreeNodes.find( text.str()) != m_ExpressionTreeNodes.end())
	{
		CExpressionTreeNode * cachedNode = &m_ExpressionTreeNodes[text.str()];
		node.m_Id = cachedNode->m_Id;
		return cachedNode;
	}
	else
	{
		node.m_Id = ++m_ExpresssionTreeNodeId;
		m_ExpressionTreeNodes[text.str()] = node;
		return &m_ExpressionTreeNodes[text.str()];
	}
}


CExpressionTreeNode *
CExporter::insertSizePrefixIfNeeded( CExpressionTreeNode * parent, ulong /*operandId*/, ulong & /*position*/ )
{
	return parent;
	// activate the following code to insert a default 32bit size prefix for all register and immediate accesses
/*
	if ( !parent->m_Id || parent->m_Type != CExpressionTreeNode::TYPE_SIZEPREFIX )
	{	// add default 32bit size prefix
		CExpressionTreeNode node;
		node.m_Type			= CExpressionTreeNode::TYPE_SIZEPREFIX;
		node.m_Parent		= parent->m_Id;
		node.m_Position		= position;
		node.m_Symbol		= "b4"; // assume 32 bit default
		node.m_Immediate	= 0;
		parent = getId( node );
		*m_OperandExpressionQuery << "(" << operandId << "," << node.m_Id << ")," << flushQuery;
		position = 0;
	}
	return parent;*/
}


ulong
CExporter::decodeOperand(
	const std::string & operand, CExpressionTreeNode * parent, ulong operandId, const t_disasm & assemblyLine, ulong position )
{
	if ( !parent )
	{
		static CExpressionTreeNode root;
		root.m_Id = 0;
		parent = &root;
	}

	ulong expressionId = 0;
	CExpressionTreeNode node;
	if ( CExpressionTreeNode::ms_TypeTable.find( operand ) != CExpressionTreeNode::ms_TypeTable.end())
	{	// register
		parent = insertSizePrefixIfNeeded( parent, operandId, position );
		node.m_Type		 = CExpressionTreeNode::ms_TypeTable[operand];
		node.m_Parent	 = parent->m_Id;
		node.m_Position  = position;
		node.m_Symbol	 = boost::to_lower_copy( operand );
		node.m_Immediate = 0;
		parent = getId( node );
		*m_OperandExpressionQuery << "(" << operandId << "," << node.m_Id << ")," << flushQuery;
		expressionId = node.m_Id;
	}
	else
	{
		size_t pos = operand.find( ' ' );
		std::string token = operand.substr( 0, pos );
		if ( CExpressionTreeNode::ms_TypeTranslation.find( token ) != CExpressionTreeNode::ms_TypeTranslation.end())
		{	// memory access specifier
			node.m_Type			= CExpressionTreeNode::TYPE_SIZEPREFIX;
			node.m_Parent		= parent->m_Id;
			node.m_Position		= position;
			node.m_Symbol		= CExpressionTreeNode::ms_TypeTranslation[token];
			node.m_Immediate	= 0;
			parent = getId( node );
			*m_OperandExpressionQuery << "(" << operandId << "," << node.m_Id << ")," << flushQuery;

			// segment selector
			pos = operand.find( ' ', pos + 1 );
			token = operand.substr( pos + 1, 2 );
			node.m_Type			= CExpressionTreeNode::TYPE_OPERATOR;
			node.m_Parent		= parent->m_Id;
			node.m_Position		= position;
			node.m_Symbol		= boost::to_lower_copy( token ) + ":";
			node.m_Immediate	= 0;
			parent = getId( node );
			*m_OperandExpressionQuery << "(" << operandId << "," << node.m_Id << ")," << flushQuery;

			// bracket operator
			node.m_Type			= CExpressionTreeNode::TYPE_DEREFERENCE;
			node.m_Parent		= parent->m_Id;
			node.m_Position		= position;
			node.m_Symbol		= "[";
			node.m_Immediate	= 0;
			parent = getId( node );
			*m_OperandExpressionQuery << "(" << operandId << "," << node.m_Id << ")," << flushQuery;

			// expression in brackets []
			pos = operand.find( '[', pos + 1 );
			token = operand.substr( pos + 1, operand.find( ']' ) - pos - 1 );
			expressionId = decodeOperand( token, parent, operandId, assemblyLine );
		}
		else
		{
			// check for mathematical expression
			size_t pos = operand.rfind( '-' );
			if ( pos == 0 )	// -4 is a number, not a math expression
				pos = std::string::npos;
			std::string operation = "+";	// we change the number's sign later on
			if ( pos == std::string::npos )
				pos = operand.find( '+' );
			if ( pos == std::string::npos )
			{
				pos = operand.find( '*' );
				operation = "*";
			}
			if ( pos != std::string::npos )
			{	// math expression
				node.m_Type			= CExpressionTreeNode::TYPE_OPERATOR;
				node.m_Parent		= parent->m_Id;
				node.m_Position		= position;
				node.m_Symbol		= operation;
				node.m_Immediate	= 0;
				parent = getId( node );
				*m_OperandExpressionQuery << "(" << operandId << "," << node.m_Id << ")," << flushQuery;
				decodeOperand( operand.substr( 0, pos ), parent, operandId, assemblyLine, 0 );

				if ( operand.find( '-' ) != std::string::npos )
					decodeOperand( "-" + operand.substr( pos + 1 ), parent, operandId, assemblyLine, 1 );
				else
					decodeOperand( operand.substr( pos + 1 ), parent, operandId, assemblyLine, 1 );
				return node.m_Id;	// @bug: is this the correct expression to return?!
			}

			if ( isNumber( operand ))
			{	// immediate
				parent = insertSizePrefixIfNeeded( parent, operandId, position );
				node.m_Type			= CExpressionTreeNode::TYPE_IMM_INT;
				node.m_Parent		= parent->m_Id;
				node.m_Position		= position;
				node.m_Symbol		= "";
				node.m_Immediate	= readNumber( operand );
				parent = getId( node );
				*m_OperandExpressionQuery << "(" << operandId << "," << node.m_Id << ")," << flushQuery;
				expressionId = node.m_Id;
			}
			else if (( pos = operand.find( '.' )) != std::string::npos
				&& isNumber( operand.substr( pos + 1 )))
			{	// address, i.e.: notepad.0x12345678
				parent = insertSizePrefixIfNeeded( parent, operandId, position );
				const ulong address = (ulong)readNumber( operand.substr( pos + 1 ));
				node.m_Type			= CExpressionTreeNode::TYPE_IMM_INT;
				node.m_Parent		= parent->m_Id;
				node.m_Position		= position;
				node.m_Immediate	= address;
				node.m_Symbol		= "";
				if ( assemblyLine.cmdtype == C_CAL )
					node.m_Symbol = getFunctionName( address );
				parent = getId( node );
				if ( assemblyLine.cmdtype == C_CAL )
					*m_ExpressionSubstitutionQuery << "(" << assemblyLine.ip
						<< "," << operandId << "," << node.m_Id << ",'"
						<< node.m_Symbol << "')," << flushQuery;
				*m_OperandExpressionQuery << "(" << operandId << "," << node.m_Id << ")," << flushQuery;
				expressionId = node.m_Id;
			}
			else
			{
				if (( assemblyLine.adrconst || operand.find( "JMP" ) != std::string::npos )
					&& operand.size() > 3 )
				{
					parent = insertSizePrefixIfNeeded( parent, operandId, position );
					node.m_Type			= CExpressionTreeNode::TYPE_IMM_INT;
					node.m_Parent		= parent->m_Id;
					node.m_Position		= position;
					node.m_Symbol		= operand;
					node.m_Immediate	= assemblyLine.adrconst ? assemblyLine.adrconst : assemblyLine.jmpconst;
					parent = getId( node );
					*m_ExpressionSubstitutionQuery << "(" << assemblyLine.ip
						<< "," << operandId << "," << node.m_Id << ",'"
						<< operand.substr( 1, operand.size() - 2 ) << "')," << flushQuery;
					expressionId = node.m_Id;
				}
				else
				{
					node.m_Type			= CExpressionTreeNode::TYPE_SYMBOL;
					node.m_Parent		= parent->m_Id;
					node.m_Position		= position;
					node.m_Symbol		= operand;
					node.m_Immediate	= 0;
					parent = getId( node );
				}
				*m_OperandExpressionQuery << "(" << operandId << "," << node.m_Id << ")," << flushQuery;
				expressionId = node.m_Id;
			}
		}
	}
	return expressionId;
}


void
CExporter::executeSqlFromFile( std::istream & source, const std::string & replacement ) const
{
	std::string query;
/*	try
	{*/
	char buffer[256];
	while ( source.getline( buffer, 256 ))
	{
		std::string line( buffer );
		size_t pos = 0;
		if (( pos = line.rfind( ';' )) != std::string::npos )
		{
			query += line.substr( 0, pos );
			boost::replace_all( query, "?", replacement );
			::executeSql( query );
			query = line.substr( pos + 1 );
		}
		else
			query += line;
	}
/*	}
	catch ( const std::exception & error )
	{		
	}*/
}


void
CExporter::writeFunctions()
{
	if ( !m_Functions.empty())
	{
		std::stringstream query;
		query
			<< "insert into ex_" << m_ModuleId << "_functions values ";
		CQueryBuilder queryBuilder( query );
		for ( TFunctions::const_iterator i = m_Functions.begin(); i != m_Functions.end(); ++i )
		{
			CFunction & function = *i->second;
			queryBuilder
				<< "(" << function.getAddress()
				<< ",'" << function.getName()
				<< "', " << function.hasRealName()
				<< ", " << function.getType();
			if ( function.getModule().empty())
				queryBuilder << ", null";
			else
				queryBuilder << ", '" << function.getModule() << "'";
			queryBuilder << ")," << flushQuery;
		}
		queryBuilder.execute();
	}
}


void
CExporter::writeCallGraph()
{
	if ( !m_CallGraphEdges.empty())
	{
		std::stringstream query;
		query << "insert into ex_" << m_ModuleId << "_callgraph (src,src_basic_block_id,src_address,dst) values ";
		CQueryBuilder queryBuilder( query );
		for ( TEdges::const_iterator i = m_CallGraphEdges.begin(); i != m_CallGraphEdges.end(); ++i )
		{
			const CEdge & edge = *i;
			const ulong functionAddress = getFunctionForAddress( edge.m_Source );
			if ( m_Functions.find( functionAddress ) == m_Functions.end())
				continue;	// @bug: this should never happen
			//assert( m_Functions.find( functionAddress ) != m_Functions.end());
			const CFunction * function = m_Functions.find( functionAddress )->second;
			const CBasicBlock * basicBlock = function->getBasicBlockForAddress( edge.m_Source );
			if ( basicBlock )
				queryBuilder
					<< "(" << functionAddress
					<< "," << ( basicBlock ? basicBlock->m_Id : 0 )
					<< "," << edge.m_Source
					<< "," << edge.m_Dest << ")," << flushQuery;
		}
		queryBuilder.execute();
	}
}


void
CExporter::writeFlowGraphs()
{
	for ( TFunctions::const_iterator i = m_Functions.begin(); i != m_Functions.end(); ++i )
		i->second->write();

	for ( TFunctions::const_iterator i = m_Functions.begin(); i != m_Functions.end(); ++i )
		if ( i->second->getNrOfBasicBlocks() == 0 )
			i->second->writeStub();

	m_BasicBlockQuery->execute();
	m_InstructionQuery->execute();
	m_FlowGraphQuery->execute();
}


void
CExporter::writeExpressionTree()
{
	if ( m_ExpressionTreeNodes.empty())
		return;

	{
	std::stringstream query;
	query << "insert into ex_" << m_ModuleId << "_expression_tree (id,expr_type,symbol,immediate,position,parent_id) values ";
	CQueryBuilder queryBuilder( query );
	for ( TExpressionTreeNodes::const_iterator i = m_ExpressionTreeNodes.begin(); i != m_ExpressionTreeNodes.end(); ++i )
	{
		const CExpressionTreeNode & node = i->second;
		queryBuilder
			<< "(" << node.m_Id
			<< "," << node.m_Type
			<< "," << ( !node.m_Symbol.empty() ? "'" + node.m_Symbol + "'" : "null" )
			<< "," << ( node.m_Type == CExpressionTreeNode::TYPE_IMM_INT ? toString( node.m_Immediate ) : "null" )
			<< "," << node.m_Position
			<< ",";
		if ( node.m_Parent != 0 )
			queryBuilder << node.m_Parent;
		else
			queryBuilder << "null";
		queryBuilder << ")," << flushQuery;
	}
	queryBuilder.execute();
	}
}


void
CExporter::writeAddressComments()
{
	if ( m_AddressComments.empty())
		return;

	std::stringstream query;
	query
		<< "insert into ex_" << m_ModuleId << "_address_comments values ";
	CQueryBuilder queryBuilder( query );
	for ( TAddressComments::iterator i = m_AddressComments.begin(); i != m_AddressComments.end(); ++i )
	{
		boost::replace_all( i->second, "'", "\"" );
		boost::replace_all( i->second, "\n", " " );
		queryBuilder << "(" << i->first << ",'" << i->second << "')," << flushQuery;
	}
	queryBuilder.execute();
}


ulong
CExporter::getFunctionForAddress( ulong address ) const
{
	TFunctions::const_iterator i = m_Functions.lower_bound( address );
	--i;
	if ( i != m_Functions.end())
		if ( i->second->addressInFunction( address ))
			return i->first;
	return 0;
}


void
CExporter::setWindowHandle( HWND window )
{
	m_WindowHandle = window;
}


HWND
CExporter::getWindowHandle() const
{
	return m_WindowHandle;
}


void
CExporter::setInstanceHandle( HINSTANCE dllInstance )
{
	m_DllInstance = dllInstance;
}


void
CExporter::showAbout() const
{
	MessageBox( m_WindowHandle, g_VersionString.c_str(),
		"Zynamics Exporter", MB_OK | MB_ICONINFORMATION );
}


void
CExporter::decodeLine( const t_disasm & assemblyLine, CFunction & function, CBasicBlock & basicBlock, ulong nextAddress )
{
	if ( strlen( assemblyLine.comment ))
		m_AddressComments[assemblyLine.ip] = assemblyLine.comment;

	const ulong expressionId = function.decodeLine( assemblyLine, *m_OperandTupleQuery, *m_OperandStringQuery, m_OperandId );
	if ( assemblyLine.cmdtype == C_JMP )
	{	// unconditional jump
		function.addBasicBlock( basicBlock );
		basicBlock.m_Address = nextAddress;
		ulong targetAddress = std::max( assemblyLine.jmpaddr, assemblyLine.jmpconst );
		if ( targetAddress != 0 )
		{
			function.addEdge( assemblyLine.ip, m_OperandId, expressionId, targetAddress, CEdge::TYPE_UNCONDITIONAL );
			m_AllEdges.push_back( CEdge( assemblyLine.ip, m_OperandId, expressionId, targetAddress, CEdge::TYPE_UNCONDITIONAL ));
			function.addBasicBlock( CBasicBlock( targetAddress ));
		}
		else	// @bug: Olly comments this case as: "Command crosses end of memory block"
		{		// I cannot decode the address, so I drop the basic block
//			std::stringstream message;
//			message
//				<< "  warning: dropping basic_block as jump target of '" << assemblyLine.result
//				<< "' address " << std::hex << assemblyLine.ip << " because OllyDbg thinks it's 0.";
//			Addtolist( 0, 0, const_cast< char * >( message.str().c_str()));
		}
	}
	else if ( assemblyLine.cmdtype == C_JMC )
	{	// conditional jump
		function.addBasicBlock( basicBlock );
		basicBlock.m_Address = nextAddress;
		ulong targetAddress = std::max( assemblyLine.jmpaddr, assemblyLine.jmpconst );
		if ( targetAddress != 0 )
		{
			function.addEdge( assemblyLine.ip, m_OperandId, expressionId, targetAddress,
				assemblyLine.condition ? CEdge::TYPE_TRUE : CEdge::TYPE_FALSE );
			m_AllEdges.push_back( CEdge( assemblyLine.ip, m_OperandId, expressionId, targetAddress,
				assemblyLine.condition ? CEdge::TYPE_TRUE : CEdge::TYPE_FALSE ));
			function.addEdge( assemblyLine.ip, 0, 0, nextAddress,
				assemblyLine.condition ? CEdge::TYPE_FALSE : CEdge::TYPE_TRUE );
			m_AllEdges.push_back( CEdge( assemblyLine.ip, 0, 0, nextAddress,
				assemblyLine.condition ? CEdge::TYPE_FALSE : CEdge::TYPE_TRUE ));
			function.addBasicBlock( CBasicBlock( targetAddress ));
		}
		else
		{
//			std::stringstream message;
//			message
//				<< "  warning: dropping basic_block as jump target of '" << assemblyLine.result
//				<< "' address " << std::hex << assemblyLine.ip << " because OllyDbg thinks it's 0.";
//			Addtolist( 0, 0, const_cast< char * >( message.str().c_str()));
		}
	}
	else if ( assemblyLine.cmdtype == C_RET )
	{	// return instruction
		function.addBasicBlock( basicBlock );
	}
	else if ( assemblyLine.cmdtype == C_CAL )	// build callgraph edges and add missing (library) functions
	{	// call instruction
		if ( assemblyLine.jmpaddr > 0 )	
		{	
			if ( m_Functions.find( assemblyLine.jmpaddr ) == m_Functions.end())
			{	// add library function
				std::string module( "" );
				std::stringstream name( assemblyLine.comment );
				unsigned type = CFunction::TYPE_IMPORTED;
				if ( name.str().empty())
				{
					name << "sub_" << std::setfill( '0' ) << std::setw( 8 ) << std::setbase( 16 ) << assemblyLine.jmpaddr;
					type = CFunction::TYPE_STANDARD;
				}
				else if ( name.str().find( "." ) != std::string::npos )
				{
					std::string temp = name.str();
					module = boost::to_lower_copy( temp.substr( 0, temp.find( "." )));
					temp = temp.substr( temp.rfind( "." ) + 1 );
					name.str( temp );
				}

				m_Functions[assemblyLine.jmpaddr] =
					new CFunction( assemblyLine.jmpaddr, assemblyLine.jmpaddr, name.str(), module, m_ModuleId, this,
					*m_InstructionQuery, *m_BasicBlockQuery, *m_FlowGraphQuery, type );
			}
			m_CallGraphEdges.push_back( CEdge( assemblyLine.ip, m_OperandId, expressionId, assemblyLine.jmpaddr, CEdge::CALL_DIRECT ));
			m_AllEdges.push_back( CEdge( assemblyLine.ip, m_OperandId, expressionId, assemblyLine.jmpaddr, CEdge::CALL_DIRECT ));
		}
		else
		{	// Olly couldn't determine target (for example "call esi")
//			std::stringstream message;
//			message << "  dropping: '" << assemblyLine.result << "'";
//			Addtolist( 0, 0, const_cast< char * >( message.str().c_str()));
		}
	}
}


void
CExporter::decodeFunction( ulong startAddress, ulong endAddress, CFunction & function )
{
	const ulong blockSize = endAddress - startAddress;
	ulong currentAddress = startAddress;
	ulong lastAddress = currentAddress;
	int stepSize = 0;
	CBasicBlock basicBlock( startAddress );

	t_disasm assemblyLine;
	do
	{
		currentAddress += stepSize;
		ulong size;
		uchar * currentDecodeInfo = Finddecode( currentAddress, &size );
		uchar currentCommand[MAXCMDSIZE];
		Readcommand( currentAddress, (char *)currentCommand );
		ulong nextAddress = Disassembleforward( NULL, startAddress, blockSize, currentAddress, 1, 1 );
		stepSize = std::max( nextAddress - currentAddress, 1ul );

		Disasm( currentCommand,	stepSize, currentAddress, currentDecodeInfo,
			&assemblyLine, DISASM_ALL, NULL );
		
		decodeLine( assemblyLine, function, basicBlock, nextAddress );
		lastAddress = currentAddress;
	}
	while ( currentAddress + stepSize < endAddress );

	if ( function.getNrOfBasicBlocks() == 0 )
	{	// happens for "no-return" functions, i.e. functions that end with a call
		function.addBasicBlock( basicBlock );
	}
}


bool	
CExporter::prepareDatabase()
{
	executeSql(
		"create table if not exists modules ( "
		"id int unsigned not null unique primary key auto_increment, "
		"name text not null, "
		"architecture varchar( 32 ) not null, "
		"base_address bigint unsigned not null, "
		"exporter varchar( 256 ) not null, "
		"version int not null, "
		"md5 char( 32 ) not null, "
		"sha1 char( 40 ) not null, "
		"comment text, "
		"import_time timestamp not null ); " );

	std::string fileName( Findmodule( reinterpret_cast< t_dump * >( Plugingetvalue( VAL_CPUDASM ))->base )->path );
	std::string md5, sha1;
	getFileHashes( fileName, md5, sha1 );
	
	g_Database << "select coalesce( max( id ) + 1, 1 ) from modules", soci::into( m_ModuleId );

	int duplicates = 0;
	g_Database << "select count(*) from modules where md5=:md5 or sha1=:sha1", soci::into( duplicates ), soci::use( md5 ), soci::use( sha1 );
	if ( duplicates > 0 )
	{
		int answer = MessageBox( getWindowHandle(),
			"module already exists, overwrite?\nYes overwrites, No creates new module id, Cancel aborts operation",
			"confirm", MB_YESNOCANCEL | MB_ICONQUESTION );
		if ( answer == IDCANCEL )
			return false;
		if ( answer == IDYES )
		{
			g_Database << "select id from modules where md5= :md5 or sha1= :sha1 limit 1", soci::into( m_ModuleId ), soci::use( md5 ), soci::use( sha1 );
			g_Database << "delete from modules where id=:id", soci::use( m_ModuleId );
		}
	}

	try
	{
		int version = 0;
		g_Database 
			<< "select count(*) from information_schema.columns where table_schema = :name "
			<< "and table_name = 'modules' and column_name = 'version'"
			, soci::use( m_DatabaseName ), soci::into( version );
		if ( version != 1 )
			throw std::runtime_error( "version error - check modules table" );

		int nrOfModules = 0;
		g_Database << "select count( * ) from modules", soci::into( nrOfModules );
		if ( nrOfModules != 0 )
		{
			g_Database << "select max( version ) - min( version ) from modules", soci::into( nrOfModules );
			if ( nrOfModules != 0 )
				throw std::runtime_error( "version error" );
			g_Database << "select version from modules limit 1", soci::into( nrOfModules );
			if ( nrOfModules != 2 )
				throw std::runtime_error( "version error" );
		}
	}
	catch ( ... )
	{
		MessageBox( getWindowHandle(),
			"expecting database version 2 - modules table already contains different version. Please chose another database.", "Notice!", MB_OK | MB_ICONEXCLAMATION );
		return false;
	}

	std::stringstream moduleId;
	moduleId << m_ModuleId;
	std::ifstream sqlFile( "InitializeTables.sql" );
	executeSqlFromFile( sqlFile, moduleId.str());

	return true;
}


void
CExporter::execute()
{
	if ( !prepareDatabase())
		throw std::string( "export cancelled" );
	reset();

	// calls analyze (I should call Analysecode directly but don't have a t_module structure)
	Sendshortcut( PM_DISASM, 0, WM_KEYDOWN, 1, 0, 'A' );
	const t_dump * disassembly = reinterpret_cast<t_dump *>( Plugingetvalue( VAL_CPUDASM ));

	Addtolist( 0, 0, "Exporting..." );
	boost::timer timer;

	ulong address = disassembly->base;
	while (( address = Findnextproc( address )) != 0 )
	{
		std::string name = getFunctionName( address );

		ulong endAddress = Findprocend( address );

		TFunctions::iterator functionIt = m_Functions.find( address );
		if ( functionIt != m_Functions.end())
		{	// already put in as a forward reference in decodeLine. Overwrite with correct entry
			delete functionIt->second;
			functionIt->second = 0;
		}
		CFunction * function = new CFunction( address, endAddress, name, "", m_ModuleId, this,
			*m_InstructionQuery, *m_BasicBlockQuery, *m_FlowGraphQuery );
		m_Functions[address] = function;

		// this gets all incoming references, mabye we should invert to outgoing refs instead?
		// currently we'll miss all library functions...
/*		Findreferences( disassembly->base, disassembly->size, address, address + 1, address, 1, "references" );
		t_table * table = reinterpret_cast< t_table * >( Plugingetvalue( VAL_REFERENCES ));
		size_t itemSize = table->data.itemsize >> 2; // itemsize is given in bytes, we want int
		for ( int i = 0; i < table->data.n; i += itemSize )
		{
			ulong xref = *((ulong *)table->data.data + i );
			if ( xref != address )	// @bug: this removes recursive calls, it's in here because Olly always thinks of the first line as a xref
				m_CallGraphEdges.push_back( CEdge( xref, address ));
		}*/

//		Addtolist( 0, 0, const_cast<char *>( name.c_str()));

		decodeFunction( function->getAddress(), function->getEndAddress(), *function );
	}

	for ( TFunctions::iterator i = m_Functions.begin(); i != m_Functions.end(); ++i )
		i->second->resolveAddressesToIds();

	// we temporarily have to disable foreign key checks because of insertion sequence. Parent_id references
	// expressions which might be inserted after this row...
	// I have expanded this over all queries for performance reasons
	executeSql( "SET FOREIGN_KEY_CHECKS=0;" );

	writeModule();
	writeFunctions();
	writeFlowGraphs();
	writeCallGraph();
	writeExpressionTree();
	writeOperands();
	writeAddressReferences();
	writeAddressComments();
	m_ExpressionSubstitutionQuery->execute();

	executeSql( "SET FOREIGN_KEY_CHECKS=1;" );

	Addtolist( 0, 0, "completed in %.2f seconds.", timer.elapsed());
}


void
CExporter::writeAddressReferences()
{
	if ( m_AllEdges.empty())
		return;

	std::stringstream query;
	query << "insert into ex_" << m_ModuleId << "_address_references (address,operand_id,expression_id,target,kind) values ";
	CQueryBuilder queryBuilder( query );
	for ( TEdges::const_iterator i = m_AllEdges.begin(); i != m_AllEdges.end(); ++i )
	{
		queryBuilder << "(" << i->m_Source;
		if ( i->m_SourceOperand )
			queryBuilder << "," << i->m_SourceOperand;
		else
			queryBuilder << ",null";
		if ( i->m_SourceExpression )
			queryBuilder << "," << i->m_SourceExpression;
		else
			queryBuilder << ",null";
		queryBuilder
			<< "," << i->m_Dest
			<< "," << i->m_Type << ")," << flushQuery;
	}
	queryBuilder.execute();
}


void
CExporter::writeModule()
{
	ulong base = reinterpret_cast< t_dump * >( Plugingetvalue( VAL_CPUDASM ))->base;
	std::string fileName( Findmodule( base )->path );
	base = Findmodule( base )->base;
	std::string md5, sha1;
	getFileHashes( fileName, md5, sha1 );

	boost::replace_all( fileName, "\\", "/" );
	const size_t pos = fileName.rfind( "/" );
	if ( pos != std::string::npos )
		fileName = fileName.substr( pos + 1 );
	std::stringstream query;
	query
		<< "insert into modules (id,name,architecture,base_address,exporter,version,md5,sha1,comment,import_time) values("
		<< m_ModuleId
		<< ",'" << fileName
		<< "','" << "x86-32"	// architecture
		<< "'," << base
		<< ",'" << g_VersionString
		<< "'," << 2			// database format version
		<< ",upper('" << md5 << "'),upper('" << sha1 << "'),'"
		<< "',now())";
	executeSql( query.str());
}


void
CExporter::writeOperands()
{
	m_OperandStringQuery->execute();
	m_OperandTupleQuery->execute();
	m_OperandExpressionQuery->execute();
}


// -----------------------------------------------------------------------------


CExporter g_Exporter;


// -----------------------------------------------------------------------------


BOOL WINAPI
DllEntryPoint( HINSTANCE /*hi*/, DWORD /*reason*/, LPVOID /*reserved*/ )
{
//	Addtolist( 0, 0, "DllEntryPoint()" );
	return 1;
}


// Report plugin name and return version of plugin interface.
extern int _export cdecl
FunctionPlugindata( char shortname[32] )
{
	strcpy_s( shortname, 32, "Zynamics Exporter" );
	return PLUGIN_VERSION;
}


// OllyDbg calls this obligatory function once during startup. I place all
// one-time initializations here. Parameter features is reserved for future
// extensions, do not use it.
extern int _export cdecl
FunctionPlugininit(
	int ollydbgversion,
	HWND window,
	ulong * /*features*/ )
{
	if ( ollydbgversion < PLUGIN_VERSION )
	   return -1;

	g_Exporter.setWindowHandle( window );

	Addtolist( 0, 0, const_cast< char * >( g_VersionString.c_str()));
	Addtolist( 0, -1, "  Copyright (C) 2008 by Zynamics GmbH" );

	return 0;
}


// This function is called each time OllyDbg passes main Windows loop.
extern void _export cdecl
FunctionPluginmainloop( DEBUG_EVENT * /*debugevent*/ )
{
//	Addtolist( 0, 0, "ODBG_Pluginmainloop()" );
}


extern void _export cdecl
FunctionPluginsaveudd( t_module * /*pmod*/, int /*ismainmodule*/ )
{
//	Addtolist( 0, 0, "ODBG_Pluginsaveudd()" );
}


extern int _export cdecl
FunctionPluginuddrecord(
	t_module * /*pmod*/,
	int /*ismainmodule*/,
	ulong /*tag*/,
	ulong /*size*/,
	void * /*data*/ )
{
//	Addtolist( 0, 0, "ODBG_Pluginuddrecord()" );
	return 1;
}


// Function adds items to main OllyDbg menu (origin=PM_MAIN).
extern int _export cdecl
FunctionPluginmenu(
	int origin,
	char data[4096],
	void * /*item*/ )
{
	if ( origin != PM_MAIN )
		return 0;                         // No pop-up menus in OllyDbg's windows

	strcpy_s( data, 4096, "0 E&xecute,1 &About" );

	return 1;
}


// Receives commands from main menu.
extern void _export cdecl
FunctionPluginaction( int origin, int action, void * /*item*/ )
{
	if ( origin != PM_MAIN )
		return;

	switch ( action )
	{
	case 0:
		try
		{
			if ( !Findmodule( reinterpret_cast< t_dump * >( Plugingetvalue( VAL_CPUDASM ))->base ))
			{
				MessageBox( g_Exporter.getWindowHandle(), "please load a module first", "Error", MB_OK | MB_ICONINFORMATION );
				return;
			}

			char text[TEXTLEN];
			if ( Gettext( "please enter database name", text, ' ', 192, FIXEDFONT ) <= 3 )
				return;

			g_Database.open( soci::mysql, g_XmlConfig.readString( "//DatabaseConnectionString/@value", "" ));
			executeSql( std::string( "create database if not exists " ) + text );
			executeSql( std::string( "use " ) + text );
			g_Exporter.setDatabaseName( boost::trim_copy( std::string( text )));
			g_Exporter.execute();
			g_Database.close();
			MessageBox( g_Exporter.getWindowHandle(), "module exported successfully", "Success", MB_OK | MB_ICONINFORMATION );
		}
		catch ( const std::string & error )
		{
			MessageBox( g_Exporter.getWindowHandle(), error.c_str(), "Error", MB_OK | MB_ICONEXCLAMATION );
			g_Database.close();
		}
		catch ( const std::exception & error )
		{
			MessageBox( g_Exporter.getWindowHandle(), error.what(), "Error", MB_OK | MB_ICONEXCLAMATION );
			g_Database.close();
		}
		catch ( ... )
		{
			MessageBox( g_Exporter.getWindowHandle(), "unknown error", "Error", MB_OK | MB_ICONEXCLAMATION );
			g_Database.close();
		}
	break;
	case 1:                            // "Help", opens help file
		g_Exporter.showAbout();
	break;
	default:
	break;
	}
}


// Command line window recognizes global shortcut Alt+F1.
extern int _export cdecl
FunctionPluginshortcut(
	int /*origin*/,
	int /*ctrl*/,
	int /*alt*/,
	int /*shift*/,
	int /*key*/,
	void * /*item*/ )
{
//	Addtolist( 0, 0, "ODBG_Pluginshortcut()" );
	return 0;                            // Shortcut not recognized
};


// User opens new or restarts current application, clear command line history.
extern void _export cdecl
FunctionPluginreset()
{
//	Addtolist( 0, 0, "ODBG_Pluginreset()" );
}


// OllyDbg calls this optional function when user wants to terminate OllyDbg.
extern int _export cdecl
FunctionPluginclose()
{
//	Addtolist( 0, 0, "ODBG_Pluginclose()" );
	return 0;
}


// OllyDbg calls this optional function once on exit. At this moment, all
// windows created by plugin are already destroyed (and received WM_DESTROY
// messages). Function must free all internally allocated resources, like
// window classes, files, memory and so on.
extern void _export cdecl
FunctionPlugindestroy()
{
//	Addtolist( 0, 0, "ODBG_Plugindestroy()" );
}


// OllyDbg calls this optional function each time the execution is paused on
// breakpoint with attached list of commands, separately for each command.
// Function must return 1 if command is processed (in this case it will not
// be passed to other plugins) and 0 otherwise.
extern int _export cdecl
FunctionPlugincmd(
	int /*reason*/,
	t_reg * /*reg*/,
	char * /*cmd*/ )
{
//	Addtolist( 0, 0, "ODBG_Plugincmd()" );
	return 0;
}
