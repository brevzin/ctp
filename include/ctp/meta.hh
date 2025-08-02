#ifndef CTP_META_HH
#define CTP_META_HH

#ifndef CTP_META_IS_STRUCTURAL

#include <algorithm>
#include <meta>

namespace std::meta {
    // this really should be in std:: but it's not yet, so...
    consteval auto is_structural_type(info type) -> bool {
        auto ctx = access_context::unchecked();

        return is_scalar_type(type)
            or is_lvalue_reference_type(type)
            or is_class_type(type)
                and ranges::all_of(bases_of(type, ctx),
                        [](info o){
                            return is_public(o)
                            and is_structural_type(type_of(o));
                        })
                and ranges::all_of(nonstatic_data_members_of(type, ctx),
                        [](info o){
                            return is_public(o)
                            and not is_mutable_member(o)
                            and is_structural_type(
                                    remove_all_extents(type_of(o)));
                        });

    }
}

#endif
#endif