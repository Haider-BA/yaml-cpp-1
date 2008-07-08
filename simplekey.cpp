#include "scanner.h"
#include "token.h"
#include "exceptions.h"
#include "exp.h"

namespace YAML
{
	Scanner::SimpleKey::SimpleKey(int pos_, int line_, int column_, int flowLevel_)
		: pos(pos_), line(line_), column(column_), flowLevel(flowLevel_), required(false), pMapStart(0), pKey(0)
	{
	}

	void Scanner::SimpleKey::Validate()
	{
		if(pMapStart)
			pMapStart->status = TS_VALID;
		if(pKey)
			pKey->status = TS_VALID;
	}

	void Scanner::SimpleKey::Invalidate()
	{
		if(required)
			throw ParserException(line, column, "required simple key not found");

		if(pMapStart)
			pMapStart->status = TS_INVALID;
		if(pKey)
			pKey->status = TS_INVALID;
	}

	// InsertSimpleKey
	// . Adds a potential simple key to the queue,
	//   and saves it on a stack.
	void Scanner::InsertSimpleKey()
	{
		SimpleKey key(INPUT.pos(), INPUT.line, INPUT.column, m_flowLevel);

		// first add a map start, if necessary
		key.pMapStart = PushIndentTo(INPUT.column, false);
		if(key.pMapStart)
			key.pMapStart->status = TS_UNVERIFIED;
//		else
//			key.required = true;	// TODO: is this correct?

		// then add the (now unverified) key
		key.pKey = new Token(TT_KEY, INPUT.line, INPUT.column);
		key.pKey->status = TS_UNVERIFIED;
		m_tokens.push(key.pKey);

		m_simpleKeys.push(key);
	}

	// VerifySimpleKey
	// . Determines whether the latest simple key to be added is valid,
	//   and if so, makes it valid.
	bool Scanner::VerifySimpleKey()
	{
		m_isLastKeyValid = false;
		if(m_simpleKeys.empty())
			return m_isLastKeyValid;

		// grab top key
		SimpleKey key = m_simpleKeys.top();

		// only validate if we're in the correct flow level
		if(key.flowLevel != m_flowLevel)
			return false;

		m_simpleKeys.pop();

		bool isValid = true;

		// needs to be followed immediately by a value
		if(m_flowLevel > 0 && !Exp::ValueInFlow.Matches(INPUT))
			isValid = false;
		if(m_flowLevel == 0 && !Exp::Value.Matches(INPUT))
			isValid = false;

		// also needs to be less than 1024 characters and inline
		if(INPUT.line != key.line || INPUT.pos() - key.pos > 1024)
			isValid = false;

		// invalidate key
		if(isValid)
			key.Validate();
		else
			key.Invalidate();

		// In block style, remember that we've pushed an indent for this potential simple key (if it was starting).
		// If it was invalid, then we need to pop it off.
		// Note: we're guaranteed to be popping the right one (i.e., there couldn't have been anything in
		//       between) since keys have to be inline, and will be invalidated immediately on a newline.
		if(!isValid && m_flowLevel == 0)
			m_indents.pop();

		m_isLastKeyValid = isValid;
		return isValid;
	}

	void Scanner::VerifyAllSimpleKeys()
	{
		while(!m_simpleKeys.empty())
			VerifySimpleKey();
	}
}
