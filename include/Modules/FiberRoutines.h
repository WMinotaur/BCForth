// ========================================================================
//
// The Forth interpreter-compiler by Prof. Boguslaw Cyganek (C) 2021
//
// The software is supplied as is and for educational purposes
// without any guarantees nor responsibility of its use in any application. 
//
// ========================================================================


#pragma once



#include <chrono>
#include <coroutine>
#include <variant>
#include <iostream>
#include <format>


#include "Words.h"





namespace BCForth
{



   auto GetTimePoint( void )
   {
 	   using timer = typename std::chrono::high_resolution_clock;
	   return std::chrono::duration_cast< std::chrono::milliseconds >( timer::now().time_since_epoch() ).count();
   }




   template <typename T>
   struct generator
   {


      // ----------------
       struct promise_type
       {

           std::variant< T const *, std::exception_ptr > value;

           promise_type & get_return_object()
           { 
               return * this;
           }
           //generator<T> get_return_object()          can be also this way, but I find it more convoluted
           //{
           //    return * this;
           //}

           std::suspend_always initial_suspend()
           {
               return {};
           }

           std::suspend_always final_suspend() noexcept        // fix
           {
               return {};
           }


           // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
           std::suspend_always yield_value( T const & other )
           {
               value = std::addressof( other );
               return {};
           }

           void return_void()      // otherwise, if we roll off the body, there will be UB ...
           {
           }

           // Included to prevent a developer from writing a co_await statement in this type of co-routine
           template < typename Expression >
           Expression && await_transform( Expression && expression )
           {
               static_assert( sizeof( expression ) == 0, "co_await is not supported in coroutines of type generator" );
               return std::forward< Expression >( expression );
           }

           void unhandled_exception()
           {
               value = std::move( std::current_exception() );
           }

           void rethrow_if_failed()
           {
               if( value.index() == 1 )
               {
                   std::rethrow_exception( std::get<1>( value ) );
               }
           }
       };
       // ----------------



       // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
       using handle_type = std::coroutine_handle< promise_type >;

       handle_type fHandle{ nullptr };




       // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
       generator( promise_type & promise ) :               
       //generator( promise_type && promise ) :                  
           fHandle( handle_type::from_promise( promise ) )
       {
       }

       generator() = default;
       generator(generator const&) = delete;
       generator &operator=(generator const&) = delete;

       generator(generator&& other) : fHandle( other.fHandle )
       {
           other.fHandle = nullptr;
       }

       generator & operator=( generator && other )
       {
           fHandle = std::exchange( other.fHandle, nullptr );
           return *this;
       }

       ~generator()
       {
           if( fHandle )
           {
               fHandle.destroy();   // important to remember! (otherwise a memory leak is generated)
           }
       }


       // -------------
       struct iterator
       {
           using iterator_category = std::input_iterator_tag;
           using value_type = T;
           using difference_type = ptrdiff_t;
           using pointer = T const*;
           using reference = T const&;

           handle_type handle;

           iterator(std::nullptr_t) : handle(nullptr)
           {
           }

           iterator(handle_type handle_type) : handle(handle_type)
           {
           }


           // To check whether this execution brought the coroutine to an end, and if so, 
           // propagate any exception that might have been raised inside the coroutine
           iterator & operator ++ ()
           {
               handle.resume();

               if ( handle.done() )
               {
                   promise_type& promise = handle.promise();
                   handle = nullptr;
                   promise.rethrow_if_failed();
               }

               return *this;
           }

           iterator operator++(int) = delete;

           bool operator==(iterator const& other) const
           {
               return handle == other.handle;
           }

           bool operator!=(iterator const& other) const
           {
               return !(*this == other);
           }

           T const& operator*() const
           {
               return * std::get<0>( handle.promise().value );     // retrieve the current value of the iterator
           }

           T const* operator->() const
           {
               return std::addressof( operator*() );       // address of whatever returned operator*
           }
       };


       // -------------
       iterator begin()            // observe that begin() is not a member of the iterator but of the generator - this
       {                           // this is to ensure proper clearing of the coroutine handle prior to throwing any uncaught exception
           if( ! fHandle )
           {
               return nullptr;
           }

           fHandle.resume();

           if( fHandle.done() )
           {
               fHandle.promise().rethrow_if_failed();
               return nullptr;
           }

           return fHandle;
       }

       iterator end()
       {
           return nullptr;
       }
       // -------------




   };







   // : MyCoRange [ 10 ]  [ 113 ]  [ 13 ]  CO_RANGE ;
   // : test ." This is test" MyCoRange CR . CR ;
   //
   // : MyCoRange2 [ -5 ]  [ +5 ]  [ 1 ]   CO_RANGE ;
   // : test2 ." This is test 2" MyCoRange2 CR . CR ;
   //
   template < typename Base >
   class CoRange : public TWord< Base >
   {
      protected:

	   using DataStack = typename Base::DataStack;
	   using TWord< Base >::GetDataStack;


      private:

         // it is assumed that the co generator ALWAYS operates on the signed int type (of a stack cell size)
	   generator< SignedIntType >	fCoRoutine;



   public:

         CoRange( Base & f ) : TWord< Base >( f ) { }


   public:

	   void operator () ( void ) override
	   {
            if( ! fCoRoutine.fHandle )          // we should leave the stack operations to the words itself
            {
                  if( typename DataStack::value_type from {}, to {}, step {}; GetDataStack().Pop( step ) && GetDataStack().Pop( to ) && GetDataStack().Pop( from ) )
                  //           vvvvvvvvv  <-- how to be able to put ANY other function here, not only range_bc3 ??
                     fCoRoutine = CreateCoRange( from, to, step );      // at the first run init reading three values from the stack
                  else
	                  throw ForthError( "unexpectedly empty stack when creating CoRange" );


                  return;
            }


            assert( fCoRoutine.fHandle );



            if( ! fCoRoutine.fHandle.done() )
            {
                  fCoRoutine.fHandle.resume();                // at first, let it yield

                  if( ! fCoRoutine.fHandle.done() )           // check again, since the co-routine could terminate
                  {
                     if( auto & value = fCoRoutine.fHandle.promise().value; value.index() == 0 )
                     {
                        auto yld_val = * std::get< 0 >( value );
                
				            GetDataStack().Push( BlindValueReInterpretation< CellType >( yld_val ) );
                     }

                  }
                  else if( auto & value = fCoRoutine.fHandle.promise().value; value.index() == 1 ) 
                  {
                     // What we hold? An exception ... ?
                     std::rethrow_exception( std::get < 1 >( value ) );
                  }
            }
            //else
            //{
                  // The range used out, what to do? ==> nothing ;)
            //}

         }



         // https://stackoverflow.com/questions/71722427/is-it-possible-to-use-co-yield-in-a-callback-that-is-entered-multiple-times 
         // Stackless coroutines can't suspend from within a function call because they don't save a separate stack for the parameters and other assorted data.
         // Good explanation:
         // https://stackoverflow.com/questions/28977302/how-do-stackless-coroutines-differ-from-stackful-coroutines
         // In contrast to a stackless coroutine a stackful coroutine can be suspended from within a nested stackframe. 
         // Execution resumes at exactly the same point in the code where it was suspended before. 
         // With a stackless coroutine, only the top-level routine may be suspended. Any routine called by that top-level routine may not itself suspend. 
         // This prohibits providing suspend/resume operations in routines within a general-purpose library.


         generator< SignedIntType > CreateCoRange( SignedIntType first, SignedIntType last, SignedIntType step )
         {
            if( first < last && step > SignedIntType( 0 ) )
            {
                  for( ; first < last;  first += step )
                     co_yield first;
                     //return YieldConveyor( first );        compiles but doesn't work
            }
            else
            {
                  if( first > last && step < SignedIntType( 0 ) )
                  {
                     for( ; first > last;  first += step )
                        co_yield first;
                        //return YieldConveyor( first );        compiles but doesn't work
                  }
                  else
                  {
                     throw ForthError( "Wrong iteration parameters when creating CoRange" );
                  }
            }
         }


   };



// ==================================================================



   struct ForthCoro 
   {

       // There is a special type required, exactly names as: promise_type.
       // The promise object is used inside the coroutine. The coroutine submits its result or exception through this object.
       // The promise type is determined by the compiler from the return type of the coroutine using std::coroutine_traits.

       struct promise_type 
       {       
           // compiler looks for `promise_type`
          // ForthCoro get_return_object() { return * this; }         // calls  ForthCoro(promise_type* p) to convert
           promise_type & get_return_object()
           { 
               return * this;
           }

           std::suspend_always initial_suspend() { return {}; }        
           std::suspend_always final_suspend() noexcept { return {}; }     // std::suspend_never


           void unhandled_exception()
           {
              throw;
           }

           void return_void()      // otherwise, if we roll off the body, there will be UB ...
           {
           }




       };

       ~ForthCoro() 
       {
          if( fHandle ) 
             fHandle.destroy(); 
       }


       ForthCoro( promise_type & promise ) : fHandle( handle_type::from_promise( promise ) )
       {
          assert( true );
       }

       ForthCoro() = default;
       ForthCoro( ForthCoro const& ) = delete;
       ForthCoro & operator = ( ForthCoro const & ) = delete;

       ForthCoro( ForthCoro && other) : fHandle( other.fHandle )
       {
           other.fHandle = nullptr;
       }

       ForthCoro & operator = ( ForthCoro && other )
       {
           fHandle = std::exchange( other.fHandle, nullptr );
           return *this;
       }



      // The coroutine handle is used to resume the execution of the coroutine or to destroy the coroutine frame. 
      // It does also indicates the status of coroutine using std::coroutine_handle::done() method.

      using handle_type = std::coroutine_handle< promise_type >;
      handle_type      fHandle;
   };



      // Co-routine based fiber - works as follows
      //              
      // : MyFiber1   [ 112 ]   [ 75 ]   LED_ONx   CO_FIBER ;
      // : LED_ONx   LED_ON   1 ;
      // 
      // 112 - a number of loop rotations
      // 75  - a time slot for the fiber (in milliseconds)
      // LED_ONx - a version of LED_ON that leaves "true" (1) on the stack
      //           otherwise the coroutine will finish
      //
      template < typename Base >
	   class CoRoFiber : public TWord< Base >
	   {
    
       protected:

		   using DataStack = typename Base::DataStack;
		   using TWord< Base >::GetDataStack;
		   using WordPtr	= Base::WordPtr;

       private:


		   ForthCoro	fCoRoutine;

         CompoWord< Base > fAssocWord;    // overtakes the "normal" word

	   public:
         using VCH = std::vector< std::coroutine_handle<> >;

       private:
          // TODO: get rid of "static"
         inline static VCH fCoroScheduler;

	   public:    

         static VCH & GetCoroScheduler() { return fCoroScheduler; }

	   public:    

         std::coroutine_handle<> GetCoroutineHandle() { return fCoRoutine.fHandle; }

	   public:

         CoRoFiber( Base & f, CompoWord< Base > && assocWord ) : TWord< Base >( f ), fAssocWord( std::move( assocWord ) ) {}


	   public:

         // When this word is called there are 2 modes of operation:
         // (1) fHandle not created - the new word is created based on the current values left on the stack
         // (2) fHandle is already created - the word is inserted into the scheduler's queue, what makes
         //     it systolically called when the scheduler is on
         //
		   void operator () ( void ) override
		   {
               if( ! fCoRoutine.fHandle )          // we should leave the stack operations to the words itself
               {
                   if( typename DataStack::value_type rotations {}, time_slice {}; GetDataStack().Pop( time_slice ) && GetDataStack().Pop( rotations ) )
                       fCoRoutine = CreateFiberCoro( rotations, time_slice, & fAssocWord );      // at the first run init reading 2 values from the stack
                   else
	                   throw ForthError( "unexpectedly empty stack when creating CoRange" );


                   return;
               }
               else
               {
                  // If the coroutine already exists, then upon the call insert it to the automatic scheduler
                  // ...

                  auto h = GetCoroutineHandle();

                  if( std::find( fCoroScheduler.begin(), fCoroScheduler.end(), h ) == fCoroScheduler.end() )
                     fCoroScheduler.push_back( h );      // insert only new objects

                  //if( ! h.done() )  // TODO: only for testing - this should be not called here but resume exclusively via the fCoroScheduler
                  //   h.resume();
               }


               assert( fCoRoutine.fHandle );

         }




         ForthCoro CreateFiberCoro( SignedIntType rotations, SignedIntType time_slice, const WordPtr p )
         {
            assert( p );
    
            auto tp0 = GetTimePoint();

            bool pre_cond { rotations == -1 ? true : false };
   
            for( int i {}; pre_cond || i < rotations; ++ i )
            {
               // execute the word and read out the top of the stack - if 0, then exit; otherwise go on
               try
               {
                  ( * p )();     // this can throw
               }
               catch( ... )
               {
                  throw;      // just re-throw 
               }

               // Check operation status - if 0 on the stack, then exit; otherwise continue until time slot
				   if( typename DataStack::value_type t {}; not GetDataStack().Pop( t ) )
					   throw ForthError( "unexpectedly empty stack - the coroutine word should leave a status value 1/0 on the stack" );
               else
					   if( t == 0 )
                     break;      // stop the coroutine


               if( GetTimePoint() - tp0 > time_slice )
               {
                  co_await std::suspend_always{};     // suspend if time elapsed
                  tp0 = GetTimePoint();
               }

            }

            co_return;
         }





   };




   auto & GetCoroScheduler()
   {
      return CoRoFiber< TForth >::GetCoroScheduler();
   }


	void ProcessCoros()
	{
		auto & sch = GetCoroScheduler();

		for( auto i { 0 }; i < sch.size(); ++ i )
		{
         auto & hdl = sch[ i ];
         if( hdl && ! hdl.done() )
            hdl.resume();
         else
            sch.erase( sch.begin() + i );
		}

	}


}







