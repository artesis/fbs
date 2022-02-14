<?php

/**
 * @file
 * Handles availability information from the library system.
 */

/**
 * Implements provider availability, items.
 *
 * @param array $provider_ids
 *   Array of ting object ids (faust).
 *
 * @return array
 *   Availability info.
 */
function fbs_availability_items($provider_ids) {
  $items = array();

  try {
    $items = fbs_service()->Catalog->getAvailability(fbs_service()->agencyId, $provider_ids, variable_get('fbs_holdings_branches_blacklist', array()));
  }
  catch (Exception $e) {
    watchdog_exception('fbs', $e);
  }

  $result = array();
  foreach ($items as $item) {
    $result[$item->recordId] = array(
      'available' => $item->available,
      'reservable' => $item->reservable,
    );
  }

  return $result;
}

/**
 * Implements provider availability, holdings.
 *
 * @param array $provider_ids
 *   Array of ting object ids (faust).
 *
 * @return array
 *   Holdings info.
 */
function fbs_availability_holdings($provider_ids) {
  $holdings = array();
  $service = fbs_service();
  if ($service) {
    try {
      $holdings = $service->Catalog->getHoldings(
        $service->agencyId,
        $provider_ids
      );
    }
    catch (Exception $e) {
      watchdog_exception('fbs', $e);
    }
  }
  $result = array();
  $tmp_issues = array();
  foreach ($holdings as $item) {
    $result[$item->recordId] = array(
      'local_id' => $item->recordId,
      'reservable' => $item->reservable,
      'show_reservation_button' => $item->reservable,
      // Gets properly set in the loop later.
      'available' => FALSE,
      'holdings' => array(),
      'total_count' => 0,
      // We don't know about this one.
      'is_internet' => FALSE,
      'is_periodical' => FALSE,
      'reservation_count' => $item->reservations,
    );
    $placement_parts = array('branch', 'department', 'location', 'sublocation');
    foreach ($item->holdings as $item_holding) {
      $result_holding = array(
        'available_count' => 0,
        'total_count' => 0,
        // We have no idea about this either.
        'reference_count' => 0,
        'placement' => array(),
      );
      // Add in placement.
      foreach ($placement_parts as $part) {
        if (!empty($item_holding->{$part}->title)) {
          $result_holding['placement'][] = $item_holding->{$part}->title;
        }
      }
      $placement_string = implode(' > ', $result_holding['placement']);
      // Count available materials.
      foreach ($item_holding->materials as $material) {
        $result[$item->recordId]['total_count']++;
        $result_holding['total_count']++;
        if ($material->available) {
          $result_holding['available_count']++;
          $result[$item->recordId]['available'] = TRUE;
        }
        if ($material->periodical) {
          // Add to issues.
          $key = (!empty($material->periodical->volumeYear) ?
              $material->periodical->volumeYear : '') . ':' .
            $material->periodical->volume . ':' .
            (!empty($material->periodical->volumeNumber) ?
              $material->periodical->volumeNumber : '') . ':' .
            $placement_string;
          if (isset($tmp_issues[$key])) {
            $tmp_issues[$key]['available'] += $material->available ? 1 : 0;
            $tmp_issues[$key]['total'] += 1;
          }
          else {
            $tmp_issues[$key] = array(
              'vol' => $material->periodical->volume . (!empty($material->periodical->volumeNumber) ? '-' . $material->periodical->volumeNumber : ''),
              'year' => $material->periodical->volumeYear,
              'available' => $material->available ? 1 : 0,
              'total' => 1,
              'placement' => $placement_string,
              'local_id' => _fbs_periodical_get_local_id(
                $item->recordId,
                $material->periodical
              ),
            );
          }
        }
      }
      $result[$item->recordId]['holdings'][] = $result_holding;
    }
    // Add issues in the right nesting.
    if (!empty($tmp_issues)) {
      $issues = array();
      $result[$item->recordId]['is_periodical'] = TRUE;
      foreach ($tmp_issues as $issue) {
        if (!isset($issues[$issue['year']])) {
          $issues[$issue['year']] = array();
        }
        $year = &$issues[$issue['year']];
        if (!isset($year[$issue['vol']])) {
          $year[$issue['vol']] = array(
            'local_id' => $issue['local_id'],
            'placement' => array(),
          );
        }
        $year[$issue['vol']]['placement'][] = array(
          'reservable' => $item->reservable,
          'available_count' => $issue['available'],
          'location' => $issue['placement'],
          'total_count' => $issue['total'],
        );
      }
      $result[$item->recordId]['issues'] = $issues;
      // Reset issues so they do not get into wrong item.
      $tmp_issues = [];
    }
    $result[$item->recordId]['html'] = _fbs_get_holdings($result[$item->recordId]);
  }
  return $result;
}

/**
 * Getting holdings for preprocessing.
 *
 * @param array $res
 *   Item array.
 *
 * @return bool|string
 *   Processed set of data.
 */
function _fbs_get_holdings($res) {
  if (isset($res)) {
    return _fbs_set_holdings($res);
  }
  return FALSE;
}

/**
 * Set holdings for all kinds of material except periodicals.
 *
 * @param array $res
 *   Item array.
 *
 * @return string
 *   Processed table markup with data.
 */
function _fbs_set_holdings($res) {
  $reservation_count = (int) $res['reservation_count'];
  return _fbs_set_table_html($res['holdings'], $reservation_count);
}

/**
 * Make the html-table.
 *
 * @param array $h
 *   Holding row data.
 * @param int $r_count
 *   Rows count.
 *
 * @return string
 *   Markup of table.
 */
function _fbs_set_table_html($h, $r_count) {
  // Set a classname for styling the table.
  $variables['attributes'] = array(
    'class' => array(
      drupal_html_class('availability_holdings_table'),
    ),
  );
  // Set table header.
  $variables['header'] = array(
    'placement' => t('Placement'),
    'copies' => t('Copies'),
    'home' => t('At home'),
  );
  // Set table rows.
  $variables['rows'] = _fbs_set_rows($h);
  // Set last row with reservation count.
  $row = array();
  $row['data']['Library'] = t('Reservations');
  $row['data']['Copies'] = array('data' => $r_count, 'colspan' => 4);
  $variables['rows'][] = $row;
  $holdings_table = array(
    '#theme' => 'table',
    '#header' => $variables['header'],
    '#rows' => $variables['rows'],
    '#attributes' => $variables['attributes'],
  );
  return drupal_render($holdings_table);
}

/**
 * Set rows in table for given holdings.
 *
 * @param array $h
 *   Holding data.
 *
 * @return array
 *   Table body.
 */
function _fbs_set_rows($h) {
  $rows = [];
  $copies_total = 0;
  $home_total = 0;
  foreach ($h as $key => $data) {
    $row = [];
    $row['placement'] = implode(' > ', $data['placement']);
    $row['copies'] = isset($data['total_count']) ? (int) $data['total_count'] : 0;
    $copies_total += $row['copies'];
    $row['home'] = isset($data['available_count']) ? (int) $data['available_count'] : 0;
    $home_total += $row['home'];
    $rows[] = $row;
  }
  // Sort library departments.
  $sort_order = variable_get('fbs_sort_departments', SORT_ASC);
  $rows = array_sort($rows, 'placement', $sort_order);
  if (count($rows) >= 1) {
    $rows = _clean_up_rows($rows);
  }
  // Adding row - totals.
  $row = [];
  $row['data']['Library'] = t('Total');
  $row['data']['Copies'] = $copies_total;
  $row['data']['Home'] = $home_total;
  $row['class'] = [drupal_html_class('availability_holdings_last_row')];
  $rows[] = $row;
  return $rows;
}

/**
 * If the same placement exists several times collect them in one line.
 *
 * @param array $_rows
 *   Placements data.
 *
 * @return array
 *   Cleaned up rows.
 */
function _clean_up_rows($_rows) {
  $rows = array();
  $placements = array();
  foreach ($_rows as $row) {
    $currkey = $row['placement'];
    if (!in_array($currkey, $placements)) {
      $placements[] = $currkey;
      $placementsarr = _get_placements_with_key($_rows, $currkey);
      $rows[] = _sum_placement($placementsarr);
    }
  }
  return $rows;
}

/**
 * Collect materials with the same placement.
 *
 * @param array $_rows
 *   Placement data.
 * @param string $currkey
 *   String with placement.
 *
 * @return array
 *   Processed row.
 */
function _get_placements_with_key($_rows, $currkey) {
  $rows = array();
  foreach ($_rows as $key) {
    if ($key['placement'] == $currkey) {
      $rows[] = $key;
    }
  }
  return $rows;
}

/**
 * Sum material for same placement in one row.
 *
 * @param array $placementsarr
 *   Holdings data.
 *
 * @return mixed
 *   Processed data.
 */
function _sum_placement($placementsarr) {
  $row = $placementsarr[0];
  $placements_count = count($placementsarr);
  for ($i = 1; $i < $placements_count; $i++) {
    $next_row = $placementsarr[$i];
    $row['copies'] += $next_row['copies'];
    $row['home'] += $next_row['home'];
  }
  return $row;
}

/**
 * Helper function for sorting nested arrays.
 *
 * @param array $array
 * @param string $on
 * @param int $order
 *
 * @return array
 */
function array_sort($array, $on, $order = SORT_ASC) {
  $new_array = array();
  $sortable_array = array();
  if (count($array) > 0) {
    foreach ($array as $k => $v) {
      if (is_array($v)) {
        foreach ($v as $k2 => $v2) {
          if ($k2 == $on) {
            $sortable_array[$k] = $v2;
          }
        }
      }
      else {
        $sortable_array[$k] = $v;
      }
    }
    switch ($order) {
      case SORT_ASC:
        asort($sortable_array);
        break;
      case SORT_DESC:
        arsort($sortable_array);
        break;
    }
    foreach ($sortable_array as $k => $v) {
      $new_array[$k] = $array[$k];
    }
  }
  return $new_array;
}
