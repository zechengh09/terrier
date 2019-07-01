#pragma once

#include <algorithm>
#include <cstdlib>
#include <string>
#include <set>

#include "catalog/catalog_defs.h"
#include "common/managed_pointer.h"
#include "parser/expression/abstract_expression.h"
#include "tuple_value_expression.h"

namespace terrier::parser {

/**
 * An utility class for Expression objects
 */
class ExpressionUtil {

 public:
  // Static utility class
  ExpressionUtil() = delete;


  /**
   * Populate the given set with all of the column oids referenced
   * in this expression tree.
   * @param col_oids
   * @param expr
   */
  static void GetColumnOids(std::set<catalog::col_oid_t> &col_oids, common::ManagedPointer<AbstractExpression> expr) {
    // Recurse into our children
    for (const auto &child: expr->GetChildren()) {
      ExpressionUtil::GetColumnOids(col_oids, child);
    }

    // If our mofo is a TupleValueExpression, then pull out our
    // column ids
    auto etype = expr->GetExpressionType();
    if (etype == ExpressionType::VALUE_TUPLE) {
      auto t_expr = expr.CastManagedPointerTo<TupleValueExpression>();
      // TODO(pavlo): Need to wait for ling to finish her PR.
    }
  }

};

}  // namespace terrier::parser
