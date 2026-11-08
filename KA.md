```
M_lex = (Q_lex, Sigma_lex, R_lex, S, F_lex)
```

```
C = { c ∈ ASCII | c ≠ LF ∧ c ≠ '"' ∧ c ≠ '\' }
M = { c ∈ ASCII | c ≠ '"' }
```
## States
```
Q_lex = {
  S,

  //Numbers (Int, Double)
  Int0,
  Int,
  U_Int,
  NumDot,
  Double,
  U_Double,
  E_Double,
  E_U_Double,
  Exp,
  S_Exp,
  DoubleFinal,

  //Strings
  Q1,
  StrEmpty,
  In,
  Esc,
  StrNormal,
  InMultiline,
  Q1Multi,
  Q2Multi,
  StrMultiline,

  //Identificators
  Underscore,
  I,

  //Others
  Q=,
  Q==,
  Q&,
  Q&&,
  Q!,
  Q!=,
  Q=>,
  Q>,
  Q<,
  Q>=,
  Q<=,
  Q<-,
  Q@,
  Q@m,
  Q@ma,
  Q@mai,
  Q@main,
  Q+,
  Q++,
  Q-,
  Q*,
  Q/,
  Q(,
  Q),
  Q:,
  Q[,
  Q],
  Q;,
  NewLine,
  NewLineSpaces,
  NewLineTabs,
  Period,
  Comma,
  Q|,
  Q||
}
```

## Transitions
```
//Numbers (Int, Double)
S -> Int0 [0]
S -> Int [1-9]
S -> Period [.]

Int0 -> NumDot [.]
Int0 -> Exp [e,E]

Int -> Int [0-9]
Int -> U_Int [_]
U_Int -> U_Int [_]
U_Int -> Int [0-9]

Int -> NumDot [.]
Int -> Exp [e,E]

NumDot -> Double [0-9]
Period -> Double [0-9]

Double -> Double [0-9]
Double -> U_Double [_]
U_Double -> U_Double [_]
U_Double -> Double [0-9]

Double -> Exp [e,E]
Exp -> E_Double [0-9]
Exp -> S_Exp [+,-]
S_Exp -> E_Double [0-9]

E_Double -> E_U_Double [_]
E_U_Double -> E_U_Double [_]
E_U_Double -> E_Double [0-9]
E_Double -> E_Double [0-9]

Double -> DoubleFinal [d,D]
E_Double -> DoubleFinal [d,D]

//Strings
S -> Q1 ["]

Q1 -> In [C]
Q1 -> Esc [\]
Q1 -> StrEmpty ["]

StrEmpty -> InMultiline ["]

In -> In [C]
In -> Esc [\]
In -> StrNormal ["]

Esc -> In [\, ", n, r, t]

InMultiline -> InMultiline [M]
InMultiline -> Q1Multi ["]

Q1Multi -> Q2Multi ["]
Q2Multi -> StrMultiline ["]

Q1Multi -> InMultiline [M]
Q2Multi -> InMultiline [M]

//Identificators
S -> I [A-Z, a-z]

S -> Underscore [_]

Underscore -> I [A-Z, a-z, 0-9, _]

I -> I [A-Z, a-z, 0-9, _]

//Others
S -> Q= [=]
Q= -> Q== [=]
S -> Q! [!]
S -> Q& [&]
Q& -> Q&& [&]
Q! -> Q!= [=]
Q= -> Q=> [>]
S -> Q> [>]
S -> Q< [<]
Q> -> Q>= [=]
Q< -> Q<= [=]
Q< -> Q<- [-]
S -> Q@ [@]
Q@ -> Q@m [m]
Q@m -> Q@ma [a]
Q@ma -> Q@mai [i]
Q@mai -> Q@main [n]
S -> Q+ [+]
Q+ -> Q++ [+]
S -> Q- [-]
S -> Q* [*]
S -> Q/ [/]
S -> Q( [(]
S -> Q) [)]
S -> Q: [:]
S -> Q[ [[]
S -> Q] []]
S -> Q; [;]
S -> NewLine [LF]
NewLine -> NewLineSpaces [ ]
NewLine -> NewLineTabs [TAB]
NewLine -> NewLine [LF]
NewLineSpaces -> NewLineSpaces [ ]
NewLineSpaces -> NewLineTabs [TAB]
NewLineSpaces -> NewLine [LF]
// NewLineTabs is non-final: a tab is valid only on a blank line.
NewLineTabs -> NewLineTabs [ , TAB]
NewLineTabs -> NewLine [LF]
S -> Comma [,]
S -> Q| [|]
Q| -> Q|| [|]
```

## Final states
```
F_lex = {
  // Numbers
  Int0,           // TOKEN_INT(value)
  Int,            // TOKEN_INT(value)
  Double,         // TOKEN_DOUBLE(value)
  E_Double,       // TOKEN_DOUBLE(value)
  DoubleFinal,    // TOKEN_DOUBLE(value)

  // Strings
  StrEmpty,       // TOKEN_STRING("")
  StrNormal,      // TOKEN_STRING(value)
  StrMultiline,   // TOKEN_STRING(value)

  // Identificators
  I,              // TOKEN_IDENTIFIER(value)
  Underscore,     // TOKEN_WILDCARD

  // Operators and special tokens
  Q=,             // TOKEN_ASSIGN
  Q==,            // TOKEN_EQUAL
  Q!,             // TOKEN_NOT
  Q&&,            // TOKEN_AND
  Q!=,            // TOKEN_NOT_EQUAL
  Q=>,            // TOKEN_ARROW
  Q>,             // TOKEN_GREATER
  Q<,             // TOKEN_LESS
  Q>=,            // TOKEN_GREATER_EQUAL
  Q<=,            // TOKEN_LESS_EQUAL
  Q<-,            // TOKEN_GENERATOR
  Q@main,         // TOKEN_MAIN_ANNOTATION

  Q+,             // TOKEN_PLUS
  Q++,            // TOKEN_CONCAT
  Q-,             // TOKEN_MINUS
  Q*,             // TOKEN_MULTIPLY
  Q/,             // TOKEN_DIVIDE

  Q(,             // TOKEN_LEFT_PAREN
  Q),             // TOKEN_RIGHT_PAREN
  Q:,             // TOKEN_COLON
  Q[,             // TOKEN_LEFT_BRACKET
  Q],             // TOKEN_RIGHT_BRACKET
  Q;,             // TOKEN_SEMICOLON
  NewLine,        // TOKEN_NEWLINE
  NewLineSpaces,  // TOKEN_NEWLINE(value includes leading spaces)
  Period,         // TOKEN_DOT
  Comma,          // TOKEN_COMMA
  Q|,             // TOKEN_UNION
  Q||             // TOKEN_OR
}
```