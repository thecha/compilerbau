/*** BNFC-Generated Visitor Design Pattern CodeGenerator. ***/
/* This implements the common visitor design pattern.
   Note that this method uses Visitor-traversal of lists, so
   List->accept() does NOT traverse the list. This allows different
   algorithms to use context information differently. */

#include <iostream>
#include "CodeGenerator.H"


CodeGenerator::CodeGenerator() :
        _Builder(llvm::getGlobalContext()) {
}

std::unique_ptr<llvm::Module>&& CodeGenerator::codegen(std::string name, Visitable* v)
{
    _Module.reset(new llvm::Module(name, llvm::getGlobalContext()));
    _namedValues.clear();
    _namedValues.push_back(AllocMap());

    v->accept(this);

    return std::move(_Module);
}


llvm::Value* CodeGenerator::visit(Visitable *v) {
    v->accept(this);
    return _value;
}


llvm::Type *CodeGenerator::getType(Type *t) {
    return t->getLLVMType();
}


void CodeGenerator::visitProgram(Program* t) {} //abstract class
void CodeGenerator::visitDef(Def* t) {} //abstract class
void CodeGenerator::visitArg(Arg* t) {} //abstract class
void CodeGenerator::visitStm(Stm* t) {} //abstract class
void CodeGenerator::visitExp(Exp* t) {} //abstract class
void CodeGenerator::visitType(Type* t) {} //abstract class

void CodeGenerator::visitPDefs(PDefs *pdefs)
{
  /* Code For PDefs Goes Here */

  pdefs->listdef_->accept(this);

}

void CodeGenerator::visitDFun(DFun *dfun)
{
    // Switch
    _namedValues.push_back(AllocMap());

    //auto args_a = visit(dfun->listarg_);

    auto args_t = std::vector<llvm::Type*>();
    auto args_n = std::vector<Id>();
    for (int i = 0; i < dfun->listarg_->size(); i++) {
        auto decl = dynamic_cast<ADecl *>((*dfun->listarg_)[i]);
        args_t.push_back(getType(decl->type_));
        args_n.push_back(decl->id_);
    }

    auto *FT = llvm::FunctionType::get(getType(dfun->type_),
            llvm::makeArrayRef(args_t), false);

    auto *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, dfun->id_, _Module.get());

    // If F conflicted, there was already something named 'Name'.  If it has a
    // body, don't allow redefinition or reextern.
    if (F->getName() != dfun->id_) {
        throw new std::runtime_error("Redefininition or overloading not allowd");
        // Delete the one we just made and get the existing one.
        /*
        F->eraseFromParent();
        F = _Module->getFunction(dfun->id_);


        // If F took a different number of args, reject.
        if (F->arg_size() != args_v.size()) {
            throw new std::runtime_error("redefinition of function with different # args");
        }

        auto y = 0;
        for (auto i = F->arg_begin(); i != F->arg_end(); i++)
        {
            if ((*i).getType()->getTypeID() == args_v[y]->getTypeID())
            {

            }
        }
         */
    }

    // Create a new basic block to start insertion into.
    auto *BB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "entry", F);
    _Builder.SetInsertPoint(BB);

    unsigned Idx = 0;
    for (llvm::Function::arg_iterator AI = F->arg_begin(); Idx != args_n.size(); ++AI, ++Idx) {
        llvm::AllocaInst* aInst = CreateEntryBlockAlloca(F, args_t[Idx], args_n[Idx]);

        _Builder.CreateStore(AI, aInst);

        _namedValues.back()[args_n[Idx]] = new AllocPair(args_t[Idx], aInst);
    }

    visit(dfun->liststm_);
    _Builder.CreateUnreachable();
    // Validate the generated code, checking for consistency.
    llvm::verifyFunction(*F);
    _value = F;


    _namedValues.pop_back();
}

void CodeGenerator::visitADecl(ADecl *adecl)
{
  /* Code For ADecl Goes Here */

  adecl->type_->accept(this);
  visitId(adecl->id_);
}

void CodeGenerator::visitSExp(SExp *sexp)
{
  /* Code For SExp Goes Here */

  _value = visit(sexp->exp_);

}

void CodeGenerator::visitSDecls(SDecls *sdecls)
{

    sdecls->type_->accept(this);
    sdecls->listid_->accept(this);

    for (int i = 0; i < sdecls->listid_->size(); i++)
    {
        auto aloc = _Builder.CreateAlloca(getType(sdecls->type_), 0, (*sdecls->listid_)[i].c_str());
        aloc->setName((*sdecls->listid_)[i].c_str());
        _namedValues.back()[(*sdecls->listid_)[i]] = new AllocPair(getType(sdecls->type_), aloc);
    }
}

void CodeGenerator::visitSInit(SInit *sinit)
{
  /* Code For SInit Goes Here */

  sinit->type_->accept(this);
  visitId(sinit->id_);
  sinit->exp_->accept(this);

    auto aloc = _Builder.CreateAlloca(getType(sinit->type_), 0, sinit->id_.c_str());
    aloc->setName(sinit->id_.c_str());

    _namedValues.back()[sinit->id_] = new AllocPair(getType(sinit->type_), aloc);

    _value = _Builder.CreateStore(visit(sinit->exp_), aloc);
}

void CodeGenerator::visitSReturn(SReturn *sreturn)
{
  /* Code For SReturn Goes Here */
    auto ret = visit(sreturn->exp_);
    if (ret->getType()->isPointerTy())
    {
        ret = _Builder.CreateLoad(ret);
    }
    _value = _Builder.CreateRet(ret);
}

void CodeGenerator::visitSReturnVoid(SReturnVoid *sreturnvoid)
{
  /* Code For SReturnVoid Goes Here */
    _value = _Builder.CreateRetVoid();
}

void CodeGenerator::visitSWhile(SWhile *swhile)
{
  /* Code For SWhile Goes Here */
    _namedValues.push_back(AllocMap());
    auto cond = visit(swhile->exp_);

    auto F = _Builder.GetInsertBlock()->getParent();

    auto LoopBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "loop", F);
    auto ContinueBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "continue");

    _Builder.CreateCondBr(cond, LoopBB, ContinueBB);

    _Builder.SetInsertPoint(LoopBB);

    visit(swhile->stm_);

    cond = visit(swhile->exp_);
    _Builder.CreateCondBr(cond, LoopBB, ContinueBB);

    F->getBasicBlockList().push_back(ContinueBB);
    _Builder.SetInsertPoint(ContinueBB);

    _namedValues.pop_back();
}

void CodeGenerator::visitSBlock(SBlock *sblock)
{
  /* Code For SBlock Goes Here */
    _namedValues.push_back(AllocMap());
    visit(sblock->liststm_);
    _namedValues.pop_back();
}

void CodeGenerator::visitSIfElse(SIfElse *sifelse)
{
  /* Code For SIfElse Goes Here */
    _namedValues.push_back(AllocMap());
    auto cond = visit(sifelse->exp_);
    if (cond->getType()->isPointerTy())
    {
        cond = _Builder.CreateLoad(cond);
    }

    auto F = _Builder.GetInsertBlock()->getParent();

// Create blocks for the then and else cases.  Insert the 'then' block at the
// end of the function.
    auto ThenBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "then", F);
    auto ElseBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "else");
    auto MergeBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "ifcont");

    _Builder.CreateCondBr(cond, ThenBB, ElseBB);

    // Then Block
    _Builder.SetInsertPoint(ThenBB);
    visit(sifelse->stm_1);
    ThenBB = _Builder.GetInsertBlock();

    _Builder.CreateBr(MergeBB);

    // Else Block
    // Emit else block.
    F->getBasicBlockList().push_back(ElseBB);
    _Builder.SetInsertPoint(ElseBB);

    visit(sifelse->stm_2);

    _Builder.CreateBr(MergeBB);

    ElseBB = _Builder.GetInsertBlock();

    // Merge Block
    F->getBasicBlockList().push_back(MergeBB);
    _Builder.SetInsertPoint(MergeBB);

    _namedValues.pop_back();
}

void CodeGenerator::visitETrue(ETrue *etrue)
{
  /* Code For ETrue Goes Here */
    _value = llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(1, 1));

}

void CodeGenerator::visitEFalse(EFalse *efalse)
{
  /* Code For EFalse Goes Here */
    _value =  llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(1, 0));
}

void CodeGenerator::visitEInt(EInt *eint)
{
  /* Code For EInt Goes Here */
    _value = llvm::ConstantInt::getSigned(llvm::Type::getInt32Ty(llvm::getGlobalContext()), eint->integer_);
}

void CodeGenerator::visitEDouble(EDouble *edouble)
{
  /* Code For EDouble Goes Here */
    _value = llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(edouble->double_));

}

void CodeGenerator::visitEString(EString *estring)
{
  /* Code For EString Goes Here */

    throw new std::invalid_argument("Strings are not supported!");
}

void CodeGenerator::visitEId(EId *eid)
{
  /* Code For EId Goes Here */

    auto var = lookup(eid->id_);
    if (var == nullptr)
        throw (new std::runtime_error("Referencing unknown Variable Name"));

    _value = var->second;
}

void CodeGenerator::visitEApp(EApp *eapp)
{
  /* Code For EApp Goes Here */

    /*auto arg_str = std::string();
    for (int i = 0; i < eapp->listexp_->size(); i++)
    {
        args_vec.push_back(args_arr + i);
        switch (args_vec[i]->getType()->getTypeID()){
            case llvm::Type::TypeID::IntegerTyID:
                arg_str += "_i";
                break;
            case llvm::Type::TypeID ::DoubleTyID:
                arg_str += "_d";
                break;
            default:
                new (std::exception
                break;
        }
    }*/

    auto Function = _Module->getFunction(eapp->id_);
    if (Function == NULL)
    {
        throw (new std::runtime_error("Trying to call unkown function"));
    }

    if (Function->arg_size() != eapp->listexp_->size())
    {
        throw (new std::runtime_error("Wrong # of arguments passed"));
    }

    auto args_vec = std::vector<llvm::Value*>();
    for (int i = 0; i < eapp->listexp_->size(); i++) {
        auto arg = visit((*eapp->listexp_)[i]);
        if (arg->getType()->isPointerTy())
        {
            arg = _Builder.CreateLoad(arg, arg->getName());
        }
        args_vec.push_back(arg);
    }

    _value = _Builder.CreateCall(Function, llvm::makeArrayRef(args_vec), "call");
}

void CodeGenerator::visitEPIncr(EPIncr *epincr)
{
    /* Code For EPIncr Goes Here */
    auto v = visit(epincr->exp_);

    if (v->getType()->getTypeID() != llvm::Type::PointerTyID)
    {
        throw new std::invalid_argument("Referencing non Pointer");
    }

    auto v1 = _Builder.CreateLoad(v, v->getName());
    llvm::Value* result;

    switch (v1->getType()->getTypeID())
    {
        case llvm::Type::IntegerTyID:
            result = _Builder.CreateAdd(v1, llvm::ConstantInt::get(llvm::getGlobalContext(),llvm::APInt(32, 1)));
            break;
        case llvm::Type::DoubleTyID:
            result = _Builder.CreateFAdd(v1, llvm::ConstantFP::get(llvm::getGlobalContext(),llvm::APFloat(1.0)));
            break;
        default:
            throw (new std::invalid_argument("Unexpected Type"));
    }

    _Builder.CreateStore(result, v);

    _value = v1;
}

void CodeGenerator::visitEPDecr(EPDecr *epdecr)
{
  /* Code For EPDecr Goes Here */
/* Code For EPIncr Goes Here */
    auto v = visit(epdecr->exp_);

    if (v->getType()->getTypeID() != llvm::Type::PointerTyID)
    {
        throw new std::invalid_argument("Referencing non Pointer");
    }

    auto v1 = _Builder.CreateLoad(v, v->getName());
    llvm::Value* result;

    switch (v1->getType()->getTypeID())
    {
        case llvm::Type::IntegerTyID:
            result = _Builder.CreateSub(v1, llvm::ConstantInt::get(llvm::getGlobalContext(),llvm::APInt(32, 1)));
            break;
        case llvm::Type::DoubleTyID:
            result = _Builder.CreateFSub(v1, llvm::ConstantFP::get(llvm::getGlobalContext(),llvm::APFloat(1.0)));
            break;
        default:
            throw (new std::invalid_argument("Unexpected Type"));
    }

    _Builder.CreateStore(result, v);

    _value = v1;
}

void CodeGenerator::visitEIncr(EIncr *eincr)
{
  /* Code For EIncr Goes Here */
    auto v = visit(eincr->exp_);

    if (v->getType()->getTypeID() != llvm::Type::PointerTyID)
    {
        throw new std::invalid_argument("Referencing non Pointer");
    }

    auto v1 = _Builder.CreateLoad(v, v->getName());
    llvm::Value* result;

    switch (v1->getType()->getTypeID())
    {
        case llvm::Type::IntegerTyID:
            result = _Builder.CreateAdd(v1, llvm::ConstantInt::get(llvm::getGlobalContext(),llvm::APInt(32, 1)));
            break;
        case llvm::Type::DoubleTyID:
            result = _Builder.CreateFAdd(v1, llvm::ConstantFP::get(llvm::getGlobalContext(),llvm::APFloat(1.0)));
            break;
        default:
            throw (new std::invalid_argument("Unexpected Type"));
    }

    _Builder.CreateStore(result, v);

    _value = result;

}

void CodeGenerator::visitEDecr(EDecr *edecr)
{
  /* Code For EDecr Goes Here */
    auto v = visit(edecr->exp_);

    if (v->getType()->getTypeID() != llvm::Type::PointerTyID)
    {
        throw new std::invalid_argument("Referencing non Pointer");
    }

    auto v1 = _Builder.CreateLoad(v, v->getName());
    llvm::Value* result;

    switch (v1->getType()->getTypeID())
    {
        case llvm::Type::IntegerTyID:
            result = _Builder.CreateSub(v1, llvm::ConstantInt::get(llvm::getGlobalContext(),llvm::APInt(32, 1)));
            break;
        case llvm::Type::DoubleTyID:
            result = _Builder.CreateFSub(v1, llvm::ConstantFP::get(llvm::getGlobalContext(),llvm::APFloat(1.0)));
            break;
        default:
            throw (new std::invalid_argument("Unexpected Type"));
    }

    _Builder.CreateStore(result, v);

    _value = result;
}

void CodeGenerator::visitETimes(ETimes *etimes)
{
    /* Code For ETimes Goes Here */
    auto op_l = visit(etimes->exp_1);
    auto op_r = visit(etimes->exp_2);

    if (op_l->getType()->isPointerTy()){
        op_l = _Builder.CreateLoad(op_l, op_l->getName());
    }
    if (op_r->getType()->isPointerTy()) {
        op_r = _Builder.CreateLoad(op_r, op_r->getName());
    }

    switch (op_l->getType()->getTypeID())
    {
        case llvm::Type::TypeID::DoubleTyID:
            _value = _Builder.CreateFMul(op_l, op_r, "mul_f");
            break;

        case llvm::Type::TypeID::IntegerTyID:
            _value = _Builder.CreateMul(op_l, op_r, "mul_i");
            break;
        default:
            throw new std::runtime_error("Unexpected Type");
    }
}

void CodeGenerator::visitEDiv(EDiv *ediv)
{
  /* Code For EDiv Goes Here */
    auto op_l = visit(ediv->exp_1);
    auto op_r = visit(ediv->exp_2);

    if (op_l->getType()->isPointerTy()){
        op_l = _Builder.CreateLoad(op_l, op_l->getName());
    }
    if (op_r->getType()->isPointerTy()) {
        op_r = _Builder.CreateLoad(op_r, op_r->getName());
    }

    switch (op_l->getType()->getTypeID())
    {
        case llvm::Type::TypeID::DoubleTyID:
            _value = _Builder.CreateFDiv(op_l, op_r, "div_f");
            break;

        case llvm::Type::TypeID::IntegerTyID:
            _value = _Builder.CreateSDiv(op_l, op_r, "div_i");
            break;
        default:
            throw new std::runtime_error("Unexpected Type");
    }
}

void CodeGenerator::visitEPlus(EPlus *eplus)
{
  /* Code For EPlus Goes Here */
    auto op_l = visit(eplus->exp_1);
    auto op_r = visit(eplus->exp_2);

    if (op_l->getType()->isPointerTy()){
        op_l = _Builder.CreateLoad(op_l, op_l->getName());
    }
    if (op_r->getType()->isPointerTy()) {
        op_r = _Builder.CreateLoad(op_r, op_r->getName());
    }



    switch (op_l->getType()->getTypeID())
    {
        case llvm::Type::TypeID::DoubleTyID:
            _value = _Builder.CreateFAdd(op_l, op_r, "plus_f");
            break;

        case llvm::Type::TypeID::IntegerTyID:
            _value = _Builder.CreateAdd(op_l, op_r, "plus_i");
            break;
        default:
            throw new std::runtime_error("Unexpected Type");
    }
}

void CodeGenerator::visitEMinus(EMinus *eminus)
{
  /* Code For EMinus Goes Here */

    auto op_l = visit(eminus->exp_1);
    auto op_r = visit(eminus->exp_2);


    if (op_l->getType()->isPointerTy()){
        op_l = _Builder.CreateLoad(op_l, op_l->getName());
    }
    if (op_r->getType()->isPointerTy()) {
        op_r = _Builder.CreateLoad(op_r, op_r->getName());
    }

    switch (op_l->getType()->getTypeID())
    {
        case llvm::Type::TypeID::DoubleTyID:
            _value = _Builder.CreateFSub(op_l, op_r, "sub_f");
            break;

        case llvm::Type::TypeID::IntegerTyID:
            _value = _Builder.CreateSub(op_l, op_r, "sub_i");
            break;
        default:
            throw new std::runtime_error("Unexpected Type");
    }

}

void CodeGenerator::visitELt(ELt *elt)
{
  /* Code For ELt Goes Here */
    auto op_l = visit(elt->exp_1);
    auto op_r = visit(elt->exp_2);


    if (op_l->getType()->isPointerTy()){
        op_l = _Builder.CreateLoad(op_l, op_l->getName());
    }
    if (op_r->getType()->isPointerTy()) {
        op_r = _Builder.CreateLoad(op_r, op_r->getName());
    }

    switch (op_l->getType()->getTypeID())
    {
        case llvm::Type::TypeID::DoubleTyID:
            _value = _Builder.CreateFCmpOLT(op_l, op_r, "lt_f");
            break;
        case llvm::Type::TypeID::IntegerTyID:
            _value = _Builder.CreateICmpSLT(op_l, op_r, "lt_i");
            break;
        default:
            throw new std::runtime_error("Unexpected Type");
    }

}

void CodeGenerator::visitEGt(EGt *egt)
{
  /* Code For EGt Goes Here */

    auto op_l = visit(egt->exp_1);
    auto op_r = visit(egt->exp_2);


    if (op_l->getType()->isPointerTy()){
        op_l = _Builder.CreateLoad(op_l, op_l->getName());
    }
    if (op_r->getType()->isPointerTy()) {
        op_r = _Builder.CreateLoad(op_r, op_r->getName());
    }


    switch (op_l->getType()->getTypeID())
    {
        case llvm::Type::TypeID::DoubleTyID:
            _value = _Builder.CreateFCmpOGT(op_l, op_r, "gt_f");
            break;

        case llvm::Type::TypeID::IntegerTyID:
            _value = _Builder.CreateICmpSGT(op_l, op_r, "gt_i");
            break;
        default:
            throw new std::runtime_error("Unexpected Type");
    }

}

void CodeGenerator::visitELtEq(ELtEq *elteq)
{
  /* Code For ELtEq Goes Here */


    auto op_l = visit(elteq->exp_1);
    auto op_r = visit(elteq->exp_2);


    if (op_l->getType()->isPointerTy()){
        op_l = _Builder.CreateLoad(op_l, op_l->getName());
    }
    if (op_r->getType()->isPointerTy()) {
        op_r = _Builder.CreateLoad(op_r, op_r->getName());
    }


    switch (op_l->getType()->getTypeID())
    {
        case llvm::Type::TypeID::DoubleTyID:
            _value = _Builder.CreateFCmpOGE(op_l, op_r, "le_f");
            break;

        case llvm::Type::TypeID::IntegerTyID:
            _value = _Builder.CreateICmpSLE(op_l, op_r, "le_i");
            break;
        default:
            throw new std::runtime_error("Unexpected Type");
    }
}

void CodeGenerator::visitEGtEq(EGtEq *egteq)
{
  /* Code For EGtEq Goes Here */

    auto op_l = visit(egteq->exp_1);
    auto op_r = visit(egteq->exp_2);


    if (op_l->getType()->isPointerTy()){
        op_l = _Builder.CreateLoad(op_l, op_l->getName());
    }
    if (op_r->getType()->isPointerTy()) {
        op_r = _Builder.CreateLoad(op_r, op_r->getName());
    }


    switch (op_l->getType()->getTypeID())
    {
        case llvm::Type::TypeID::DoubleTyID:
            _value = _Builder.CreateFCmpOGE(op_l, op_r, "ge_f");
            break;

        case llvm::Type::TypeID::IntegerTyID:
            _value = _Builder.CreateICmpSGE(op_l, op_r, "ge_i");
            break;
        default:
            throw new std::runtime_error("Unexpected Type");
    }
}

void CodeGenerator::visitEEq(EEq *eeq)
{
  /* Code For EEq Goes Here */

    auto op_l = visit(eeq->exp_1);
    auto op_r = visit(eeq->exp_2);


    if (op_l->getType()->isPointerTy()){
        op_l = _Builder.CreateLoad(op_l, op_l->getName());
    }
    if (op_r->getType()->isPointerTy()) {
        op_r = _Builder.CreateLoad(op_r, op_r->getName());
    }


    switch (op_l->getType()->getTypeID())
    {
        case llvm::Type::TypeID::DoubleTyID:
            _value = _Builder.CreateFCmpOEQ(op_l, op_r, "eq_f");
            break;

        case llvm::Type::TypeID::IntegerTyID:
            _value = _Builder.CreateICmpEQ(op_l, op_r, "eq_i");
            break;
        default:
            throw new std::runtime_error("Unexpected Type");
    }

}

void CodeGenerator::visitENEq(ENEq *eneq)
{
  /* Code For ENEq Goes Here */
    auto op_l = visit(eneq->exp_1);
    auto op_r = visit(eneq->exp_2);


    if (op_l->getType()->isPointerTy()){
        op_l = _Builder.CreateLoad(op_l, op_l->getName());
    }
    if (op_r->getType()->isPointerTy()) {
        op_r = _Builder.CreateLoad(op_r, op_r->getName());
    }


    switch (op_l->getType()->getTypeID())
    {
        case llvm::Type::TypeID::DoubleTyID:
            _value = _Builder.CreateFCmpONE(op_l, op_r, "neq_f");
            break;

        case llvm::Type::TypeID::IntegerTyID:
            _value = _Builder.CreateICmpNE(op_l, op_r, "neq_i");
            break;
        default:
            throw new std::runtime_error("Unexpected Type");
    }
}

void CodeGenerator::visitEAnd(EAnd *eand)
{
  /* Code For EAnd Goes Here */
    auto op_l = visit(eand->exp_1);
    auto op_r = visit(eand->exp_2);


    if (op_l->getType()->isPointerTy()){
        op_l = _Builder.CreateLoad(op_l, op_l->getName());
    }
    if (op_r->getType()->isPointerTy()) {
        op_r = _Builder.CreateLoad(op_r, op_r->getName());
    }


    switch (op_l->getType()->getTypeID())
    {
        case llvm::Type::TypeID::IntegerTyID:
            _value = _Builder.CreateAnd(op_l, op_r, "and");
            break;
        default:
            throw new std::runtime_error("Unexpected Type");
    }
}

void CodeGenerator::visitEOr(EOr *eor)
{
  /* Code For EOr Goes Here */
    auto op_l = visit(eor->exp_1);
    auto op_r = visit(eor->exp_2);


    if (op_l->getType()->isPointerTy()){
        op_l = _Builder.CreateLoad(op_l, op_l->getName());
    }
    if (op_r->getType()->isPointerTy()) {
        op_r = _Builder.CreateLoad(op_r, op_r->getName());
    }


    switch (op_l->getType()->getTypeID())
    {
        case llvm::Type::TypeID::IntegerTyID:
            _value = _Builder.CreateOr(op_l, op_r, "or");
            break;
        default:
            throw new std::runtime_error("Unexpected Type");
    }
}

void CodeGenerator::visitEAss(EAss *eass)
{
  /* Code For EAss Goes Here */

    auto op_l = visit(eass->exp_1);
    auto op_r = visit(eass->exp_2);


    if (!op_l->getType()->isPointerTy()){
        throw new std::invalid_argument("Expected Variable");
    }
    if (op_r->getType()->isPointerTy()) {
        op_r = _Builder.CreateLoad(op_r, op_r->getName());
    }

    _Builder.CreateStore(op_r, op_l);

    _value =  op_r;
    /*
  eass->exp_1->accept(this);
  eass->exp_2->accept(this);
*/
}

void CodeGenerator::visitETyped(ETyped *etyped)
{
  /* Code For ETyped Goes Here */

  etyped->exp_->accept(this);
  etyped->type_->accept(this);

}

void CodeGenerator::visitType_bool(Type_bool *type_bool)
{
  /* Code For Type_bool Goes Here */
	//???
	//curType = llvm::Type::getInt1Ty(TheContext);

}

void CodeGenerator::visitType_int(Type_int *type_int)
{
  /* Code For Type_int Goes Here */
	//_types = {llvm::Type::getInt32Ty(TheContext)};

}

void CodeGenerator::visitType_double(Type_double *type_double)
{
  /* Code For Type_double Goes Here */
	//_types = {llvm::Type::getDoubleTy(TheContext)};

}

void CodeGenerator::visitType_void(Type_void *type_void)
{
  /* Code For Type_void Goes Here */
	//_types = {llvm::Type::getVoidTy(TheContext)};

}

void CodeGenerator::visitType_string(Type_string *type_string)
{
  /* Code For Type_string Goes Here */


}


void CodeGenerator::visitListDef(ListDef* listdef)
{
  for (ListDef::iterator i = listdef->begin() ; i != listdef->end() ; ++i)
  {
    (*i)->accept(this);
  }
}

void CodeGenerator::visitListArg(ListArg* listarg)
{
	//auto types = std::vector<llvm::Type*>();
	 for (ListArg::iterator i = listarg->begin() ; i != listarg->end() ; ++i)
	 {
		(*i)->accept(this);
		//types.push_back(_types[0]);
	  }
	 // _types = types;
}

void CodeGenerator::visitListStm(ListStm* liststm)
{
    for (ListStm::iterator i = liststm->begin() ; i != liststm->end(); ++i)
    {
        visit((*i));
    }
}

void CodeGenerator::visitListExp(ListExp* listexp)
{
  for (ListExp::iterator i = listexp->begin() ; i != listexp->end() ; ++i)
  {
    (*i)->accept(this);
  }
}

void CodeGenerator::visitListId(ListId* listid)
{
  for (ListId::iterator i = listid->begin() ; i != listid->end() ; ++i)
  {
    visitId(*i) ;
  }
}


void CodeGenerator::visitId(Id x)
{
  /* Code for Id Goes Here */
}

void CodeGenerator::visitInteger(Integer x)
{
  /* Code for Integer Goes Here */
}

void CodeGenerator::visitChar(Char x)
{
  /* Code for Char Goes Here */
}

void CodeGenerator::visitDouble(Double x)
{
  /* Code for Double Goes Here */
}

void CodeGenerator::visitString(String x)
{
  /* Code for String Goes Here */
}

void CodeGenerator::visitIdent(Ident x)
{
  /* Code for Ident Goes Here */
}





