{-# LANGUAGE ForeignFunctionInterface #-}
{-# LANGUAGE OverloadedStrings #-}
{-# LANGUAGE RecursiveDo #-}

module Main where

import qualified Data.Text.Lazy.IO as T

import qualified LLVM.AST as AST hiding (function)
import qualified LLVM.AST.Constant as C
import qualified LLVM.AST.Float as F
import LLVM.AST.Type as AST
import LLVM.Pretty -- from the llvm-hs-pretty package

import qualified Data.ByteString as B hiding (putStrLn)
import qualified Data.ByteString.Char8 as C
import Data.ByteString.UTF8
import Data.Map as M
import Foreign.C.String
import LLVM.AST.CallingConvention as CallConv
import LLVM.AST.InlineAssembly
import qualified LLVM.CodeGenOpt as CodeGenOpt
import qualified LLVM.CodeModel as CodeModel
import qualified LLVM.ExecutionEngine as EE
import LLVM.IRBuilder.Constant
import LLVM.IRBuilder.Instruction
import LLVM.IRBuilder.Module
import LLVM.IRBuilder.Monad
import LLVM.Internal.Context
import LLVM.Internal.Target
import LLVM.Module
import qualified LLVM.Relocation as Reloc
import Numeric (showHex)
import System.IO.Unsafe
import Text.Printf

data Expr
    = Number Integer
    | Add Expr Expr
    | Mul Expr Expr
    deriving (Show, Eq, Ord)

expr :: Expr
expr = Add (Number 1) (Number 2)

alloc :: MonadIRBuilder m => Type -> m AST.Operand
alloc t = alloca t Nothing 8

gen :: MonadIRBuilder m => Expr -> m AST.Operand
gen (Number i) = int32 i
gen (Add e1 e2) = do
    a <- gen e1
    b <- gen e2
    add a b
gen (Mul e1 e2) = do
    a <- gen e1
    b <- gen e2
    mul a b

convert :: [Integer] -> Expr
convert = Prelude.foldl (\l r -> Add (Number r) l) (Number 1)

withTM :: (TargetMachine -> IO a) -> IO a
withTM f = do
    initializeAllTargets
    triple <- getProcessTargetTriple
    cpu <- getHostCPUName
    features <- getHostCPUFeatures
    (target, _) <- lookupTarget Nothing triple
    withTargetOptions $ \options
    -- this is ugly, i know
     ->
        withTargetMachine
            target
            triple
            cpu
            features
            options
            Reloc.Default
            CodeModel.Default
            CodeGenOpt.None
            f

bs2Hex :: B.ByteString -> String
bs2Hex = concatMap ((\x -> "0x" ++ x ++ " ") . flip showHex "") . B.unpack

toModule :: Expr -> AST.Module
toModule e =
    buildModule "basic" $
    mdo function "foobar" [] i32 $ \_ ->
            mdo entry <- block `named` "entry"
                do val <- gen e
                   ret val

codegen :: AST.Module -> IO ()
codegen mod =
    withContext $ \context ->
        withModuleFromAST context mod $ \m ->
            withTM $ \tm -> do
                o <- moduleTargetAssembly tm m
                C.putStrLn o
                return ()

foreign import ccall unsafe "mobo_run_vm" moboRunVM :: CString -> Int

data VMResult
    = VMOkay
    | VMUnhandledException Int
    deriving (Show)

runVM :: String -> VMResult
runVM path =
    let runResult =
            unsafePerformIO $ do
                cpath <- newCString path
                return $ moboRunVM cpath
     in case runResult of
            0 -> VMOkay
            _ -> VMUnhandledException runResult

main :: IO ()
main = do
    print $ runVM "build/kernel.elf"


