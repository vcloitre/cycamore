// recipe_reactor_tests.cc
#include <gtest/gtest.h>

#include "recipe_reactor.h"
#include "error.h"
#include "message.h"
#include "facility_model_tests.h"
#include "model_tests.h"

#include <string>
#include <queue>

using namespace std;

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class FakeRecipeReactor : public RecipeReactor {
  public:
    FakeRecipeReactor() : RecipeReactor() {

      setCycleLength(3);

      CompMap test_comp;

      int u235 = 92235;
      double one = 1.0;
      test_comp[u235]=one;
      string test_mat_unit = "test_mat_unit";
      string test_rec_name = "test_rec_name";
      double test_size = 10.0;
      bool test_template = true;
      IsoVector recipe(test_comp);
      recipe.setMass(test_size);

      SetCapacity(2);
      setInventorySize(50);

      // initialize ordinary objects
      setFacLife(10);
      SetCapacityFactor(.9);

      // all facilities require commodities - possibly many
      string recipe_name;
      std::string in_commod;
      std::string out_commod;
      IsoVector in_recipe;
      IsoVector out_recipe;

      // for each fuel pair, there is an in and an out commodity
      int pairs = 1;
      for (int i = 0; i < pairs; i++){
        // get commods
        out_commod = "out-commod";
        in_commod = "in-commod";

        // get in_recipe
        in_recipe = recipe;

        // get out_recipe
        out_recipe = recipe;

        addFuelPair(in_commod, in_recipe, out_commod, out_recipe);
      }
    }

    virtual ~FakeRecipeReactor() { }
};

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
cyclus::Model* RecipeReactorModelConstructor(){
  return dynamic_cast<cyclus::Model*>(new FakeRecipeReactor());
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
cyclus::FacilityModel* RecipeReactorConstructor(){
  return dynamic_cast<cyclus::FacilityModel*>(new FakeRecipeReactor());
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
class RecipeReactorTest : public ::testing::Test {
  protected:
    FakeRecipeReactor* src_facility;
    FakeRecipeReactor* new_facility;
    TestMarket* out_market_;
    TestMarket* in_market_;

    virtual void SetUp(){
      src_facility = new FakeRecipeReactor();
      src_facility->SetParent(new TestInst());
      new_facility = new FakeRecipeReactor();
      in_market_ = new TestMarket(src_facility->inCommod());
      out_market_ = new TestMarket(src_facility->outCommod());
    }

    virtual void TearDown() {
      delete src_facility;
      delete new_facility;
      delete in_market_;
      delete out_market_;
    }
};


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(RecipeReactorTest, InitialState) {
  // Test things about the initial state of the facility here
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(RecipeReactorTest, CopyFreshModel) {
  new_facility->copyFreshModel(dynamic_cast<cyclus::Model*>(src_facility)); // deep copy
  EXPECT_NO_THROW(dynamic_cast<RecipeReactor*>(new_facility)); // still a recipe reactor
  EXPECT_NO_THROW(dynamic_cast<FakeRecipeReactor*>(new_facility)); // still a fake recipe reactor
  // Test that RecipeReactor specific parameters are initialized in the deep copy method here
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(RecipeReactorTest, Print) {
  EXPECT_NO_THROW(std::string s = src_facility->str());
  // Test RecipeReactor specific aspects of the print method here
}


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(RecipeReactorTest, ReceiveMessage) {
  cyclus::Message::Ptr msg;
  // Test RecipeReactor specific behaviors of the ReceiveMessage function here
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(RecipeReactorTest, Tick) {
  int time = 1;
  EXPECT_NO_THROW(src_facility->HandleTick(time));
  // Test RecipeReactor specific behaviors of the HandleTick function here
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_F(RecipeReactorTest, Tock) {
  int time = 1;
  EXPECT_NO_THROW(src_facility->HandleTick(time));
  // Test RecipeReactor specific behaviors of the HandleTock function here
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
INSTANTIATE_TEST_CASE_P(RecipeReactor, FacilityModelTests, Values(&RecipeReactorConstructor));
INSTANTIATE_TEST_CASE_P(RecipeReactor, ModelTests, Values(&RecipeReactorModelConstructor));

