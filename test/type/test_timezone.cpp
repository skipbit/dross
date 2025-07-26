#include <gtest/gtest.h>
#include <dross/type/timezone.h>
#include <string>
#include <sstream>

// =============================================================================
// Factory Methods
// =============================================================================

TEST(timezone_test, utc_factory) {
    dross::timezone utc = dross::timezone::utc();
    
    EXPECT_TRUE(utc.is_utc());
    EXPECT_EQ(utc.offset().count(), 0);
    EXPECT_EQ(utc.format(), "Z");
}


TEST(timezone_test, offset_factory_positive) {
    dross::timezone jst = dross::timezone::offset(9);
    
    EXPECT_FALSE(jst.is_utc());
    EXPECT_EQ(jst.offset().count(), 540);
    EXPECT_EQ(jst.format(), "+09:00");
}

TEST(timezone_test, offset_factory_negative) {
    dross::timezone pst = dross::timezone::offset(-8);
    
    EXPECT_FALSE(pst.is_utc());
    EXPECT_EQ(pst.offset().count(), -480);
    EXPECT_EQ(pst.format(), "-08:00");
}

TEST(timezone_test, offset_factory_with_minutes) {
    dross::timezone ist = dross::timezone::offset(5, 30);
    
    EXPECT_FALSE(ist.is_utc());
    EXPECT_EQ(ist.offset().count(), 330);
    EXPECT_EQ(ist.format(), "+05:30");
}

TEST(timezone_test, offset_factory_negative_with_minutes) {
    dross::timezone nfld = dross::timezone::offset(-3, 30);
    
    EXPECT_FALSE(nfld.is_utc());
    EXPECT_EQ(nfld.offset().count(), -210);
    EXPECT_EQ(nfld.format(), "-03:30");
}

TEST(timezone_test, offset_factory_chrono_based) {
    using namespace std::chrono;
    
    // Basic chrono::minutes usage
    dross::timezone jst = dross::timezone::offset(minutes(540));
    dross::timezone ist = dross::timezone::offset(hours(5) + minutes(30));
    dross::timezone pst = dross::timezone::offset(minutes(-480));
    
    EXPECT_FALSE(jst.is_utc());
    EXPECT_EQ(jst.offset().count(), 540);
    EXPECT_EQ(jst.format(), "+09:00");
    
    EXPECT_FALSE(ist.is_utc());
    EXPECT_EQ(ist.offset().count(), 330);
    EXPECT_EQ(ist.format(), "+05:30");
    
    EXPECT_FALSE(pst.is_utc());
    EXPECT_EQ(pst.offset().count(), -480);
    EXPECT_EQ(pst.format(), "-08:00");
}

TEST(timezone_test, offset_factory_chrono_invalid) {
    using namespace std::chrono;
    
    // Test out-of-range values
    dross::timezone invalid1 = dross::timezone::offset(minutes(15 * 60));   // +15:00
    dross::timezone invalid2 = dross::timezone::offset(minutes(-13 * 60));  // -13:00
    
    // Should fallback to UTC
    EXPECT_TRUE(invalid1.is_utc());
    EXPECT_TRUE(invalid2.is_utc());
}

TEST(timezone_test, offset_factory_invalid_hours) {
    dross::timezone invalid1 = dross::timezone::offset(15);  // Too large
    dross::timezone invalid2 = dross::timezone::offset(-13); // Too small
    
    // Should fallback to UTC
    EXPECT_TRUE(invalid1.is_utc());
    EXPECT_TRUE(invalid2.is_utc());
}

TEST(timezone_test, offset_factory_invalid_minutes) {
    dross::timezone invalid1 = dross::timezone::offset(5, 60);  // Minutes too large
    dross::timezone invalid2 = dross::timezone::offset(5, -1);  // Minutes negative
    
    // Should fallback to UTC
    EXPECT_TRUE(invalid1.is_utc());
    EXPECT_TRUE(invalid2.is_utc());
}

// =============================================================================
// String Parsing
// =============================================================================

TEST(timezone_test, from_string_z) {
    auto tz1 = dross::timezone::from_string("Z");
    auto tz2 = dross::timezone::from_string("z");
    
    ASSERT_TRUE(tz1.has_value());
    ASSERT_TRUE(tz2.has_value());
    EXPECT_TRUE(tz1->is_utc());
    EXPECT_TRUE(tz2->is_utc());
    EXPECT_EQ(tz1->format(), "Z");
    EXPECT_EQ(tz2->format(), "Z");
}

TEST(timezone_test, from_string_positive_offset) {
    auto tz1 = dross::timezone::from_string("+09:00");
    auto tz2 = dross::timezone::from_string("+0530");
    
    ASSERT_TRUE(tz1.has_value());
    ASSERT_TRUE(tz2.has_value());
    EXPECT_EQ(tz1->offset().count(), 540);
    EXPECT_EQ(tz2->offset().count(), 330);
    EXPECT_EQ(tz1->format(), "+09:00");
    EXPECT_EQ(tz2->format(), "+05:30");
}

TEST(timezone_test, from_string_negative_offset) {
    auto tz1 = dross::timezone::from_string("-08:00");
    auto tz2 = dross::timezone::from_string("-0430");
    
    ASSERT_TRUE(tz1.has_value());
    ASSERT_TRUE(tz2.has_value());
    EXPECT_EQ(tz1->offset().count(), -480);
    EXPECT_EQ(tz2->offset().count(), -270);
    EXPECT_EQ(tz1->format(), "-08:00");
    EXPECT_EQ(tz2->format(), "-04:30");
}

TEST(timezone_test, from_string_invalid) {
    auto tz1 = dross::timezone::from_string("invalid");
    auto tz2 = dross::timezone::from_string("+25:00");
    auto tz3 = dross::timezone::from_string("-08:60");
    
    // Should return nullopt for invalid strings
    EXPECT_FALSE(tz1.has_value());
    EXPECT_FALSE(tz2.has_value());
    EXPECT_FALSE(tz3.has_value());
}

// =============================================================================
// Copy and Assignment
// =============================================================================

TEST(timezone_test, copy_constructor) {
    dross::timezone original = dross::timezone::offset(9);
    dross::timezone copy(original);
    
    EXPECT_EQ(original, copy);
    EXPECT_EQ(copy.offset().count(), 540);
    EXPECT_EQ(copy.format(), "+09:00");
}

TEST(timezone_test, assignment_operator) {
    dross::timezone tz1 = dross::timezone::utc();
    dross::timezone tz2 = dross::timezone::offset(9);
    
    tz1 = tz2;
    EXPECT_EQ(tz1, tz2);
    EXPECT_EQ(tz1.offset().count(), 540);
    EXPECT_EQ(tz1.format(), "+09:00");
}

TEST(timezone_test, self_assignment) {
    dross::timezone tz = dross::timezone::offset(9);
    dross::timezone& ref = tz;
    tz = ref;  // Self-assignment
    
    EXPECT_EQ(tz.offset().count(), 540);
    EXPECT_EQ(tz.format(), "+09:00");
}

// =============================================================================
// Comparison Operations
// =============================================================================

TEST(timezone_test, equality) {
    dross::timezone utc1 = dross::timezone::utc();
    dross::timezone utc2 = dross::timezone::offset(0);
    dross::timezone jst1 = dross::timezone::offset(9);
    dross::timezone jst2 = dross::timezone::offset(9, 0);
    dross::timezone local1 = dross::timezone::utc();
    dross::timezone local2 = dross::timezone::utc();
    
    EXPECT_EQ(utc1, utc2);
    EXPECT_EQ(jst1, jst2);
    EXPECT_EQ(local1, local2);
    
    EXPECT_NE(utc1, jst1);
    EXPECT_EQ(utc1, local1);  // Local variables now use UTC
    EXPECT_NE(jst1, local1);
}

TEST(timezone_test, three_way_comparison) {
    dross::timezone pst = dross::timezone::offset(-8);
    dross::timezone utc = dross::timezone::utc();
    dross::timezone jst = dross::timezone::offset(9);
    dross::timezone local = dross::timezone::utc();
    
    // Offset comparison
    EXPECT_LT(pst, utc);
    EXPECT_LT(utc, jst);
    EXPECT_GT(jst, utc);
    EXPECT_GT(utc, pst);
    
    // All local references now point to UTC
    EXPECT_EQ(local, utc);
    
    // Equal comparisons
    EXPECT_EQ(pst <=> pst, std::strong_ordering::equal);
    EXPECT_EQ(local <=> dross::timezone::utc(), std::strong_ordering::equal);
}

// =============================================================================
// String Conversion
// =============================================================================

TEST(timezone_test, string_conversion) {
    dross::timezone utc = dross::timezone::utc();
    dross::timezone jst = dross::timezone::offset(9);
    dross::timezone ist = dross::timezone::offset(5, 30);
    dross::timezone pst = dross::timezone::offset(-8);
    dross::timezone local = dross::timezone::utc();
    
    // Implicit conversion
    std::string utc_str = utc;
    std::string jst_str = jst;
    std::string ist_str = ist;
    std::string pst_str = pst;
    std::string local_str = local;
    
    EXPECT_EQ(utc_str, "Z");
    EXPECT_EQ(jst_str, "+09:00");
    EXPECT_EQ(ist_str, "+05:30");
    EXPECT_EQ(pst_str, "-08:00");
    EXPECT_EQ(local_str, "Z");  // Local variables now use UTC
}

TEST(timezone_test, format_method) {
    dross::timezone utc = dross::timezone::utc();
    dross::timezone jst = dross::timezone::offset(9);
    dross::timezone local = dross::timezone::utc();
    
    EXPECT_EQ(utc.format(), "Z");
    EXPECT_EQ(jst.format(), "+09:00");
    EXPECT_EQ(local.format(), "Z");  // Local variables now use UTC
}

// =============================================================================
// Stream Output
// =============================================================================

TEST(timezone_test, stream_output) {
    dross::timezone utc = dross::timezone::utc();
    dross::timezone jst = dross::timezone::offset(9);
    dross::timezone local = dross::timezone::utc();
    
    std::ostringstream oss_utc;
    oss_utc << utc;
    EXPECT_EQ(oss_utc.str(), "Z");
    
    std::ostringstream oss_jst;
    oss_jst << jst;
    EXPECT_EQ(oss_jst.str(), "+09:00");
    
    std::ostringstream oss_local;
    oss_local << local;
    EXPECT_EQ(oss_local.str(), "Z");  // Local variables now use UTC
    
    // Multiple values
    std::ostringstream oss_multiple;
    oss_multiple << jst << " and " << utc;
    EXPECT_EQ(oss_multiple.str(), "+09:00 and Z");
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST(timezone_test, extreme_offsets) {
    dross::timezone max_east = dross::timezone::offset(14);  // +14:00 (Kiritimati)
    dross::timezone max_west = dross::timezone::offset(-12); // -12:00 (Baker Island)
    
    EXPECT_EQ(max_east.offset().count(), 840);
    EXPECT_EQ(max_west.offset().count(), -720);
    EXPECT_EQ(max_east.format(), "+14:00");
    EXPECT_EQ(max_west.format(), "-12:00");
}

TEST(timezone_test, zero_offset_vs_utc) {
    dross::timezone utc = dross::timezone::utc();
    dross::timezone zero_offset = dross::timezone::offset(0);
    
    EXPECT_EQ(utc, zero_offset);
    EXPECT_TRUE(utc.is_utc());
    EXPECT_TRUE(zero_offset.is_utc());
    EXPECT_EQ(utc.format(), "Z");
    EXPECT_EQ(zero_offset.format(), "Z");
}