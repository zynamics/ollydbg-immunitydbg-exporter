#ifndef MAIN_HPP
#define MAIN_HPP 1
#pragma once


#pragma warning ( push, 0 )
#include <boost/cstdint.hpp>
#pragma warning ( pop )


// -----------------------------------------------------------------------------


class CTerminator
{
};


class CQueryBuilder
{
public:
	explicit CQueryBuilder( std::stringstream & basequery, size_t querySize = 0 );
	void execute();

private:
	std::string					m_BaseQuery;
	std::vector< std::string >	m_Queries;
	std::stringstream			m_CurrentQuery;
	size_t						m_QuerySize;

	friend CQueryBuilder & operator << ( CQueryBuilder &, const std::string & );
	friend CQueryBuilder & operator << ( CQueryBuilder &, ulong );
	friend CQueryBuilder & operator << ( CQueryBuilder &, const CTerminator & );
};


// -----------------------------------------------------------------------------


class CExpressionTreeNode
{
public:
	enum
	{
		TYPE_MNEMONIC	= 0,
		TYPE_SYMBOL		= 1,
		TYPE_IMM_INT	= 2,
		TYPE_IMM_FLOAT  = 3,
		TYPE_OPERATOR	= 4,
		TYPE_REGISTER   = 5,
		TYPE_SIZEPREFIX = 6,
		TYPE_DEREFERENCE= 7
	};


	CExpressionTreeNode();
	CExpressionTreeNode( unsigned id, int type, const std::string & symbol,	boost::int64_t immediate, int position, unsigned parent	);

	typedef std::map< std::string, int > TTypeTable;
	typedef std::map< std::string, std::string > TTypeTranslation;

	static TTypeTable initTypeTable();
	static TTypeTranslation initTypeTranslation();

	unsigned		m_Id;
	int				m_Type;
	std::string		m_Symbol;
	boost::int64_t	m_Immediate;
	int				m_Position;
	unsigned		m_Parent;
	static TTypeTable		ms_TypeTable;
	static TTypeTranslation ms_TypeTranslation;
};


// -----------------------------------------------------------------------------


class CBasicBlock
{
public:
	CBasicBlock();
	explicit CBasicBlock( ulong startAddress );
	void nextId();

	ulong m_Address;
	ulong m_Id;
	static ulong ms_NextId;
};


// -----------------------------------------------------------------------------


class CLine
{
public:
	CLine( ulong address, const std::string & mnemonic, const std::string & binaryDump, bool isFlow )
		: m_Address( address )
		, m_Mnemonic( mnemonic )
		, m_BinaryDump( binaryDump )
		, m_IsFlow( isFlow )
	{
	}

	ulong			m_Address;
	std::string		m_Mnemonic;
	std::string		m_BinaryDump;
	bool			m_IsFlow;
};


// -----------------------------------------------------------------------------


class CEdge
{
public:
	typedef enum TEdgeType
	{	
		TYPE_TRUE				= 0,
		TYPE_FALSE				= 1,
		TYPE_UNCONDITIONAL		= 2,
		TYPE_SWITCH				= 3,
		CALL_DIRECT             = 4,
		CALL_INDIRECT           = 5,
		CALL_INDIRECT_VIRTUAL   = 6,
		DATA                    = 7,
		DATA_STRING             = 8
	};

	CEdge( ulong source,  ulong operandId, ulong expressionId, ulong dest, unsigned type )
		: m_Source( source )
		, m_SourceOperand( operandId )
		, m_SourceExpression( expressionId )
		, m_Dest( dest )
		, m_Type( type )
	{
	}

	ulong		m_Source;
	ulong		m_SourceOperand;
	ulong		m_SourceExpression;
	ulong		m_Dest;
	unsigned	m_Type;
};


// -----------------------------------------------------------------------------


class CExporter;

class CFunction
{
public:
	enum
	{
		TYPE_STANDARD	= 0,
		TYPE_LIBRARY	= 1,
		TYPE_IMPORTED   = 2,
		TYPE_THUNK      = 3
	};

	CFunction( ulong startAddress, ulong endAddress,
		const std::string & name, const std::string & module, int moduleId, CExporter * exporter,
		CQueryBuilder & instructionQuery,
		CQueryBuilder & basicBlockQuery,
		CQueryBuilder & flowgraphQuery,
		unsigned type = TYPE_STANDARD );

	bool		addressInFunction( ulong address ) const;
	std::string getName() const;
	std::string getModule() const;
	bool		hasRealName() const;
	ulong		getAddress() const;
	ulong		getEndAddress() const;
	unsigned	getType() const;
	void		addBasicBlock( const CBasicBlock & basicBlock );
	void		addEdge( ulong source, ulong sourceOperand, ulong sourceExpression, ulong dest, CEdge::TEdgeType type );
	void		write();
	ulong		decodeLine( const t_disasm & assemblyLine, CQueryBuilder & tupleQuery, CQueryBuilder & stringQuery, ulong & operandId );
	void		resolveAddressesToIds();
	unsigned	getNrOfBasicBlocks() const;
	void		writeStub();
	const CBasicBlock * getBasicBlockForAddress( ulong address ) const;

private:
	typedef std::map< ulong, CBasicBlock > TBasicBlocks;
	typedef std::vector< CEdge > TEdges;
	typedef std::vector< CLine > TLines;

	std::string			m_Name;
	std::string			m_Module;
	ulong				m_StartAddress;
	ulong				m_EndAddress;
	unsigned			m_Type;
	TBasicBlocks		m_BasicBlocks;
	TEdges				m_Edges;
	int					m_ModuleId;
	TLines				m_Lines;
	CExporter *			m_Exporter;
	CQueryBuilder &		m_InstructionQuery;
	CQueryBuilder &		m_BasicBlockQuery;
	CQueryBuilder &		m_FlowGraphQuery;

	void resolveEdges();

	CFunction & operator = ( const CFunction & );
};


// -----------------------------------------------------------------------------


class CExporter
{
public:
	CExporter();
	~CExporter();

	void showAbout() const;
	void setWindowHandle( HWND window );
	HWND getWindowHandle() const;
	void setInstanceHandle( HINSTANCE instance );
	void execute();
	void reset();
	ulong decodeOperand( const std::string & operand, CExpressionTreeNode * parent, ulong operandId, const t_disasm & assemblyLine, ulong position = 0 );
	void setDatabaseName( const std::string & name );

private:
	typedef std::map< ulong, CFunction * > TFunctions;
	typedef std::vector< CEdge > TEdges;
	typedef std::map< std::string, CExpressionTreeNode > TExpressionTreeNodes;
	typedef std::map< ulong, std::string > TAddressComments;

	HINSTANCE				m_DllInstance;
	HWND					m_WindowHandle;
	TFunctions				m_Functions;
	TEdges					m_CallGraphEdges;
	TEdges					m_AllEdges;
	TExpressionTreeNodes	m_ExpressionTreeNodes;
	int						m_ModuleId;
	unsigned				m_ExpresssionTreeNodeId;
	CQueryBuilder *			m_OperandTupleQuery;
	CQueryBuilder * 		m_OperandStringQuery;
	CQueryBuilder *			m_OperandExpressionQuery;
	CQueryBuilder *			m_InstructionQuery;
	CQueryBuilder *			m_BasicBlockQuery;
	CQueryBuilder *			m_FlowGraphQuery;
	CQueryBuilder *			m_ExpressionSubstitutionQuery;
	ulong					m_OperandId;
	TAddressComments		m_AddressComments;
	std::string				m_DatabaseName;

	ulong	getFunctionForAddress( ulong address ) const;
	void	decodeLine( const t_disasm & assemblyLine, CFunction & function, CBasicBlock & basicBlock, ulong nextAddress );
	void	decodeFunction( ulong startAddress, ulong endAddress, CFunction & function );
	void	executeSqlFromFile( std::istream & source, const std::string & replacement ) const;
	bool	prepareDatabase();
	void	writeFunctions();
	void	writeCallGraph();
	void	writeFlowGraphs();
	void	writeExpressionTree();
	void	writeOperands();
	void	writeModule();
	void	writeAddressReferences();
	void	writeAddressComments();

	CExpressionTreeNode * insertSizePrefixIfNeeded( CExpressionTreeNode * parent, ulong operandId, ulong & position );
	CExpressionTreeNode * getId( CExpressionTreeNode & node );
};


// -----------------------------------------------------------------------------


#endif
